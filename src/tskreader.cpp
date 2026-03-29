#include "tskreader.h"

#include "blocksequence_impl.h"
#include "entry.h"
#include "inode.h"
#include "inodeandblocktracker.h"
#include "inodeandblocktrackerimpl.h"
#include "inputhandler.h"
#include "outputhandler.h"
#include "progressinfo.h"
#include "readseek_impl.h"

#include "tskconversion.h"
#include "tskfacade.h"
#include "tsktimestamps.h"

TskReader::TskReader(const std::string& imgPath):
  ImgPath(imgPath),
  Img(nullptr, nullptr),
  Input(),
  Tsk(new TskFacade),
  Asm(),
  Tsg(nullptr),
  RecHasher(),
  Dirents(RecHasher)
{
}

TskReader::~TskReader() {}

bool TskReader::open() {
  return bool(Img = Tsk->openImg(ImgPath.c_str()));
}

bool TskReader::startReading() {
  Asm.addImage(Tsk->convertImg(*Img));

  // tell TskAuto to start giving files to processFile
  // std::cerr << "Image is " << getImageSize() << " bytes in size" << std::endl;
  const bool ret = Tsk->walk(
    Img.get(),
    [this](const TSK_VS_INFO* vs_info) { return filterVs(vs_info); },
    [this](const TSK_VS_PART_INFO* vs_part) { return filterVol(vs_part); },
    [this](TSK_FS_INFO* fs_info) { return filterFs(fs_info); },
    [this](TSK_FS_FILE* fs_file, const char* path) { return processFile(fs_file, path); }
  );

  // wrap up the walk
  while (!Dirents.empty()) {
    Input->push(Dirents.pop());
  }

  // teardown
  Input->flush();
  Input->flushAllBuckets();

  return ret;
}

TSK_FILTER_ENUM TskReader::filterVs(const TSK_VS_INFO* vs_info) {
  Asm.addVolumeSystem(Tsk->convertVS(*vs_info));
  return TSK_FILTER_CONT;
}

TSK_FILTER_ENUM TskReader::filterVol(const TSK_VS_PART_INFO* vs_part) {
  Asm.addVolume(Tsk->convertVol(*vs_part));
  return TSK_FILTER_CONT;
}

TSK_FILTER_ENUM TskReader::filterFs(TSK_FS_INFO* fs_info) {
  Asm.addFileSystem(Tsk->convertFS(*fs_info));
  Tsg = Tsk->makeTimestampGetter(fs_info->ftype);
//  Tracker->setInodeRange(fs_info->first_inum, fs_info->last_inum + 1);
//  Tracker->setBlockRange(fs_info->first_block * fs_info->block_size, (fs_info->last_block + 1) * fs_info->block_size);
  CurFsOffset = fs_info->offset;
  CurFsBlockSize = fs_info->block_size;
  InodeTracker.clear();
  InodeTracker.resize(fs_info->last_inum - fs_info->first_inum + 1, false);
  if (Progress) {
    Progress->setFilesystem(++FsIndex, 0, fs_info->inum_count,
                            static_cast<uint64_t>(fs_info->block_count) * fs_info->block_size);
  }
  Input->startFilesystem(static_cast<uint64_t>(fs_info->block_count) * fs_info->block_size);
  return TSK_FILTER_CONT;
}

TSK_RETVAL_ENUM TskReader::processFile(TSK_FS_FILE* fs_file, const char* path) {
  addToBatch(fs_file, path);
  return TSK_OK;
}

bool TskReader::addToBatch(TSK_FS_FILE* fs_file, const char* path) {
  if (!fs_file || !fs_file->meta) {
    // TODO: Can we have a nonull fs_file->name in this case?
    // nothing to process
    // I mean, yes? of course? obviously? TSK can possibly recover deleted dirents that point to nonexistent inodes -- jls
    // so, this is wrong and needs to be a unit test
    return false;
  }
  const TSK_FS_META& meta = *fs_file->meta;
  if (!InodeTracker.at(meta.addr - fs_file->fs_info->first_inum)) {
    Inode inode;
    TskUtils::convertMetaToInode(meta, *Tsg, inode);
    inode.FsOffset = CurFsOffset;

    // handle the attrs
    Tsk->populateAttrs(fs_file);

    /*TskReaderHelper::handleAttrs(
      meta, CurFsOffset, CurFsBlockSize, inum, *Tsk, *Tracker, jmeta["attrs"]
    );*/
    // why on earth are we making this separate block sequence thing? it's goofy -- jls
    //Input->push({std::move(jmeta), makeBlockSequence(fs_file)});

    Input->push(inode);
    auto entry = std::make_unique<Entry>(meta.addr);
    entry->EvidenceFile = ImgPath;
    entry->FsIndex = FsIndex;
    entry->FsOffset = CurFsOffset;
    entry->FsType = fs_file->fs_info->ftype;
    entry->AddrFlags = meta.flags;
    entry->Path = path ? path : "";
    entry->FileSize = meta.size;

    // Extract first non-resident data run offset for disk-locality bucketing
    if (meta.attr) {
      for (const TSK_FS_ATTR* attr = meta.attr->head; attr; attr = attr->next) {
        if ((attr->type == TSK_FS_ATTR_TYPE_NTFS_DATA ||
             attr->type == TSK_FS_ATTR_TYPE_DEFAULT) &&
            (attr->flags & TSK_FS_ATTR_NONRES) && attr->nrd.run) {
          entry->DiskOffset = attr->nrd.run->addr * CurFsBlockSize;
          break;
        }
      }
    }

    Input->push(std::move(entry));
    InodeTracker.at(meta.addr - fs_file->fs_info->first_inum) = true;
  }
  // handle the name
  if (fs_file->name) {
    const TSK_INUM_T parentAddr =  fs_file->name->par_addr;
    Dirent dirent;

    if (!Dirents.empty() && parentAddr != Dirents.top().MetaAddr) {
      do {
        Input->push(Dirents.pop());
      } while (!Dirents.empty() && parentAddr != Dirents.top().MetaAddr);
      Input->maybeFlush();
    }
    // std::cerr << par_addr << " -> " << fs_file->meta->addr << '\n';
    TskUtils::convertNameToDirent("", *fs_file->name, dirent);
    auto immediate = Dirents.push(std::move(dirent));
    if (immediate) {
      Input->push(*immediate);
    }
  }

  return true;
}

std::shared_ptr<BlockSequence> TskReader::makeBlockSequence(TSK_FS_FILE* fs_file) {
  TSK_FS_INFO* their_fs = fs_file->fs_info;

  // open our own copy of the fs, since TskAuto closes the ones it opens
  auto [itr, absent] = Fs.try_emplace(their_fs->offset, nullptr);
  if (absent) {
    itr->second.reset(Tsk->openFS(Img.get(), their_fs->offset, their_fs->ftype).release(), tsk_fs_close);
  }
  TSK_FS_INFO* our_fs = itr->second.get();

  // open our own copy of the file, since TskAuto closes the ones it opens
  return std::static_pointer_cast<BlockSequence>(
    std::make_shared<TskBlockSequence>(
      Tsk->openFile(our_fs, fs_file->meta->addr)
    )
  );
}

std::unique_ptr<ReadSeek> TskReader::makeReadSeek(TSK_FS_FILE* fs_file) {
  TSK_FS_INFO* their_fs = fs_file->fs_info;

  // open our own copy of the fs, since TskAuto closes the ones it opens
  auto [itr, absent] = Fs.try_emplace(their_fs->offset, nullptr);
  if (absent) {
    itr->second.reset(Tsk->openFS(Img.get(), their_fs->offset, their_fs->ftype).release(), tsk_fs_close);
  }
  return std::make_unique<ReadSeekTSK>(
    // each file has a shared_ptr to its fs, so it can be opened on demand
    itr->second, fs_file->meta->addr
  );
}

