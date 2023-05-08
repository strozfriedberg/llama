#pragma once

#include <map>
#include <memory>
#include <stack>
#include <vector>

#include <tsk/libtsk.h>

#include "direntstack.h"
#include "filerecord.h"
#include "hex.h"
#include "inodeandblocktracker.h"
#include "inodeandblocktrackerimpl.h"
#include "inputhandler.h"
#include "inputreader.h"
#include "outputhandler.h"
#include "recordhasher.h"
#include "tskfacade.h"
#include "tskimgassembler.h"
#include "tskreaderhelper.h"
#include "tsktimestamps.h"
#include "util.h"

class BlockSequence;

class TskReader: public InputReader {
public:
  TskReader(const std::string& imgPath);

  virtual ~TskReader() {}

  bool open();

  virtual void setInputHandler(std::shared_ptr<InputHandler> in) override {
    Input = in;
  }

  virtual void setOutputHandler(std::shared_ptr<OutputHandler> out) override {
    Output = out;
  }

  virtual bool startReading() override;

private:
  // callbacks
  TSK_FILTER_ENUM filterVs(const TSK_VS_INFO* vs_info);
  TSK_FILTER_ENUM filterVol(const TSK_VS_PART_INFO* vs_part);
  TSK_FILTER_ENUM filterFs(TSK_FS_INFO* fs_info);
  TSK_RETVAL_ENUM processFile(TSK_FS_FILE* fs_file, const char* /* path */);

  bool addToBatch(TSK_FS_FILE* fs_file);

  std::shared_ptr<BlockSequence> makeBlockSequence(TSK_FS_FILE* fs_file) {
    TSK_FS_INFO* their_fs = fs_file->fs_info;

    // open our own copy of the fs, since TskAuto closes the ones it opens
    auto [i, absent] = Fs.try_emplace(their_fs->offset, nullptr, nullptr);
    if (absent) {
      i->second = Tsk->openFS(
        Img.get(), their_fs->offset, their_fs->ftype
      );
    }
    TSK_FS_INFO* our_fs = i->second.get();

    // open our own copy of the file, since TskAuto closes the ones it opens
    return std::static_pointer_cast<BlockSequence>(
      std::make_shared<TskBlockSequence>(
        Tsk->openFile(our_fs, fs_file->meta->addr)
      )
    );
  }

  std::string ImgPath;
  std::unique_ptr<TSK_IMG_INFO, void(*)(TSK_IMG_INFO*)> Img;
  std::map<TSK_OFF_T, std::unique_ptr<TSK_FS_INFO, void(*)(TSK_FS_INFO*)>> Fs;

  std::shared_ptr<InputHandler> Input;
  std::shared_ptr<OutputHandler> Output;

  std::unique_ptr<TskFacade> Tsk;
  TskImgAssembler Asm;
  std::unique_ptr<TimestampGetter> Tsg;
  std::unique_ptr<InodeAndBlockTracker> Tracker;

  RecordHasher RecHasher;
  DirentStack Dirents;

  uint64_t CurFsOffset;
  uint64_t CurFsBlockSize;
};
