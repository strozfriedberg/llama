#pragma once

#include <map>
#include <memory>
#include <stack>
#include <unordered_map>
#include <vector>

#include "tsk.h"

#include "direntstack.h"
#include "filerecord.h"
#include "hex.h"
#include "inputreader.h"
#include "recordhasher.h"
#include "readseek.h"
#include "tskimgassembler.h"
#include "tskreaderhelper.h"
#include "util.h"

class BlockSequence;
class InputHandler;
class OutputHandler;
class TimestampGetter;

class TskReader: public InputReader {
public:
  TskReader(const std::string& imgPath);

  virtual ~TskReader();

  bool open();

  virtual void setInputHandler(const std::shared_ptr<InputHandler>& in) override {
    Input = in;
  }

  virtual void setOutputHandler(const std::shared_ptr<OutputHandler>&) override {
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
  std::unique_ptr<ReadSeek> makeReadSeek(TSK_FS_FILE* fs_file);

  std::string ImgPath;
  std::unique_ptr<TSK_IMG_INFO, void(*)(TSK_IMG_INFO*)> Img;
  std::unordered_map<TSK_OFF_T, std::shared_ptr<TSK_FS_INFO>> Fs;

  std::shared_ptr<InputHandler> Input;

  std::unique_ptr<TskFacade> Tsk;
  TskImgAssembler Asm;
  std::unique_ptr<TimestampGetter> Tsg;

  std::vector<bool> InodeTracker;

  RecordHasher RecHasher;
  DirentStack Dirents;

  uint64_t CurFsOffset;
  uint64_t CurFsBlockSize;
};

