#pragma once

#include "readseek.h"
#include "tsk.h"

class Entry {
public:
  Entry(uint64_t addr) : Addr(addr) {}
  Entry(uint64_t addr, std::unique_ptr<ReadSeek> rs) : Addr(addr), stream(std::move(rs)) {}
  ReadSeek& getStream() { return *stream; }
  void setStream(std::unique_ptr<ReadSeek> rs) { stream = std::move(rs); }

  uint64_t Addr;
  std::string EvidenceFile;
  uint32_t    FsIndex = 0;
  uint64_t    FsOffset = 0;
  uint32_t    AddrFlags = 0;
  std::string Path;
  uint64_t    FileSize = 0;
  TSK_FS_TYPE_ENUM FsType = TSK_FS_TYPE_DETECT;

private:
  std::unique_ptr<ReadSeek> stream;
};