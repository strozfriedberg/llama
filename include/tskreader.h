#pragma once

#include <map>
#include <memory>
#include <stack>
#include <vector>

#include <tsk/libtsk.h>

#include "direntstack.h"
#include "filerecord.h"
#include "hex.h"
#include "inputhandler.h"
#include "inputreader.h"
#include "outputhandler.h"
#include "recordhasher.h"
#include "tskimgassembler.h"
#include "tskreaderhelper.h"
#include "util.h"

class BlockSequence;
class InodeAndBlockTracker;
class TimestampGetter;

class TskReader: public InputReader {
public:
  TskReader(const std::string& imgPath);

  virtual ~TskReader();

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

  std::shared_ptr<BlockSequence> makeBlockSequence(TSK_FS_FILE* fs_file);

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
