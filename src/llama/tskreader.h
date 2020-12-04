#pragma once

#include <stack>
#include <vector>

#include <tsk/libtsk.h>

#include "inputreader.h"
#include "tskconversion.h"

class InputHandler;

class TSKReader : public InputReader, public TskAuto {
public:
  TSKReader(const std::string& imgName);

  virtual ~TSKReader() {}

  bool open();

  void setInumRange(uint64_t begin, uint64_t end);

  virtual void setInputHandler(std::shared_ptr<InputHandler> in) override;

  virtual void setOutputHandler(std::shared_ptr<OutputHandler> in) override;

  virtual bool startReading() override;

  // recurseDisk wraps TskAuto::findFilesInImg(). Override to replace/mock.
  virtual bool recurseDisk();

  bool addToBatch(TSK_FS_FILE* fs_file);

  //
  // from TskAuto
  //
  virtual TSK_RETVAL_ENUM processFile(TSK_FS_FILE* fs_file,
                                      const char* path) override;

  virtual TSK_FILTER_ENUM filterVs(const TSK_VS_INFO* vs_info) override;

  virtual TSK_FILTER_ENUM filterVol(const TSK_VS_PART_INFO* vs_part) override;

  virtual TSK_FILTER_ENUM filterFs(TSK_FS_INFO* fs_info) override;

private:
  std::string ImgName;

  std::shared_ptr<InputHandler> Input;
  std::shared_ptr<OutputHandler> Output;

  const TSK_FS_INFO* LastFS;

  std::vector<bool> InodeEncountered;

  uint64_t InumBegin,
           InumEnd;

  TskConverter Conv;

  std::stack<TSK_INUM_T> Path;
};
