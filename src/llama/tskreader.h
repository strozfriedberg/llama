#pragma once

#include <map>
#include <memory>
#include <stack>
#include <vector>

#include <tsk/libtsk.h>

#include "filerecord.h"
#include "inputhandler.h"
#include "inputreader.h"
#include "outputhandler.h"
#include "tsk.h"
#include "tskimgassembler.h"
#include "util.h"

class BlockSequence;
class TimestampGetter;

template <class Provider>
class TskReader: public InputReader {
public:
  TskReader(const std::string& imgPath):
    ImgPath(imgPath),
    Img(nullptr, nullptr),
    Input(),
    Output(),
    Tsk(),
    Ass(),
    Tsg(nullptr)
  {
  }

  virtual ~TskReader() {}

  bool open() {
    return bool(Img = Tsk.openImg(ImgPath.c_str()));
  }

  virtual void setInputHandler(std::shared_ptr<InputHandler> in) override {
    Input = in;
  }

  virtual void setOutputHandler(std::shared_ptr<OutputHandler> out) override {
    Output = out;
  }

  virtual bool startReading() override {
    Ass.addImage(Tsk.convertImg(*Img));

    // tell TskAuto to start giving files to processFile
    // std::cerr << "Image is " << getImageSize() << " bytes in size" << std::endl;

    const bool ret = Tsk.walk(
      Img.get(),
      [this](const TSK_VS_INFO* vs_info) { return filterVs(vs_info); },
      [this](const TSK_VS_PART_INFO* vs_part) { return filterVol(vs_part); },
      [this](TSK_FS_INFO* fs_info) { return filterFs(fs_info); },
      [this](TSK_FS_FILE* fs_file, const char* path) { return processFile(fs_file, path); }
    );

    if (ret) {
      while (!Path.empty()) {
        std::cerr << Path.top() << " done\n";
        Path.pop();
      }

      Output->outputImage(Ass.dump());

      // teardown
      Input->flush();
    }
    return ret;
  }

  bool addToBatch(TSK_FS_FILE* fs_file) {
    if (!fs_file || !fs_file->meta) {
      return false;
    }

  //  const uint64_t index = fs_file->meta->addr - InumBegin;
    const uint64_t inum = fs_file->meta->addr;

  //  std::cerr << fs_file->meta->addr << ' ' << InumBegin << ' ' << index << ' ' << InodeEncountered.size() << std::endl;

    if (markInodeSeen(inum)) {
      return false;
    }

    //
    // handle the name
    //
    if (fs_file->name) {
      while (!Path.empty() && fs_file->name->par_addr != Path.top()) {
        std::cerr << Path.top() << " done\n";
        Path.pop();
      }
      Path.push(fs_file->meta->addr);

      std::cerr << fs_file->name->par_addr << " -> " << Path.top() << '\n';

      Output->outputDirent(Tsk.convertName(*fs_file->name));
    }

    //
    // handle the meta
    //
    jsoncons::json meta = Tsk.convertMeta(*fs_file->meta, *Tsg);
    jsoncons::json& attrs = meta["attrs"];

    // ridiculous bullshit to force attrs to be populated
    Tsk.populateAttrs(fs_file);

    // handle the attrs
    if (fs_file->meta->attr) {
      for (const TSK_FS_ATTR* a = fs_file->meta->attr->head; a; a = a->next) {
        if (a->flags & TSK_FS_ATTR_INUSE) {
          attrs.push_back(Tsk.convertAttr(*a));
        }
      }
    }

    Input->push({std::move(meta), makeBlockSequence(fs_file)});

    return true;
  }

  void setInodeRange(uint64_t begin, uint64_t end) {
    InumBegin = begin;
    InumEnd = end;
// FIXME: Apparently "first_inum" is first in some way other than the usual
// meaning of first, because it's 2 on DadeMurphy but we still see inode 0
// there. WTF? For the time being, just waste a few bits at the start of the
// encountered vector.
//  InodeEncountered.resize(end - begin);

    InodeEncountered.clear();
    InodeEncountered.resize(end+1);
  }

  void setBlockRange(uint64_t begin, uint64_t end) {
    // FIXME: unclear if we can rely on end - begin + 1 to be the actual count
    BlockBegin = begin;
    BlockEnd = end;
    Allocated.clear();
    Allocated.resize(end+1);
  }

  bool markInodeSeen(uint64_t inum) {
    // TODO: bounds checking? inum could be bogus
    if (InodeEncountered[inum]) {
      return true;
    }
    else {
      InodeEncountered[inum] = true;
      return false;
    }
  }

  void claimBlockRange(uint64_t begin, uint64_t end) {

  }

  // callbacks
  TSK_FILTER_ENUM filterVs(const TSK_VS_INFO* vs_info) {
    Ass.addVolumeSystem(Tsk.convertVS(*vs_info));
    return TSK_FILTER_CONT;
  }

  TSK_FILTER_ENUM filterVol(const TSK_VS_PART_INFO* vs_part) {
    Ass.addVolume(Tsk.convertVol(*vs_part));
    return TSK_FILTER_CONT;
  }

  TSK_FILTER_ENUM filterFs(TSK_FS_INFO* fs_info) {
    Ass.addFileSystem(Tsk.convertFS(*fs_info));
    Tsg = Tsk.makeTimestampGetter(fs_info->ftype);
    setInodeRange(fs_info->first_inum, fs_info->last_inum);
    return TSK_FILTER_CONT;
  }

  TSK_RETVAL_ENUM processFile(TSK_FS_FILE* fs_file, const char* /* path */) {
    // std::cerr << "processFile " << path << "/" << fs_file->name->name << std::endl;
    addToBatch(fs_file);
    return TSK_OK;
  }

private:
  std::shared_ptr<BlockSequence> makeBlockSequence(TSK_FS_FILE* fs_file) {
    TSK_FS_INFO* their_fs = fs_file->fs_info;

    // open our own copy of the fs, since TskAuto closes the ones it opens
    auto [i, absent] = Fs.try_emplace(their_fs->offset, nullptr, nullptr);
    if (absent) {
      i->second = Tsk.openFS(
        Img.get(), their_fs->offset, their_fs->ftype
      );
    }

    TSK_FS_INFO* our_fs = i->second.get();

    // open our own copy of the file, since TskAuto closes the ones it opens
    return std::static_pointer_cast<BlockSequence>(
      std::make_shared<TskBlockSequence>(
        Tsk.openFile(our_fs, fs_file->meta->addr)
      )
    );
  }

  std::string ImgPath;
  std::unique_ptr<TSK_IMG_INFO, void(*)(TSK_IMG_INFO*)> Img;
  std::map<TSK_OFF_T, std::unique_ptr<TSK_FS_INFO, void(*)(TSK_FS_INFO*)>> Fs;

  std::shared_ptr<InputHandler> Input;
  std::shared_ptr<OutputHandler> Output;

  std::vector<bool> InodeEncountered;

  uint64_t InumBegin,
           InumEnd;

  std::vector<bool> Allocated;

  uint64_t BlockBegin,
           BlockEnd;

  Provider Tsk;
  TskImgAssembler Ass;
  std::unique_ptr<TimestampGetter> Tsg;

  std::stack<TSK_INUM_T> Path;
};
