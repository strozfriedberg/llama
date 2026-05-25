#pragma once

#include <array>
#include <cstdint>

#include "readseek.h"
#include "tsk.h"

class Entry {
public:
  Entry(uint64_t addr) : Addr(addr) {}
  Entry(uint64_t addr, std::unique_ptr<ReadSeek> rs) : Addr(addr), stream(std::move(rs)) {}
  ReadSeek& getStream() { return *stream; }
  void setStream(std::unique_ptr<ReadSeek> rs) { stream = std::move(rs); }
  bool hasStream() const { return stream != nullptr; }

  uint64_t Addr;
  std::string EvidenceFile;
  uint32_t    FsIndex = 0;
  uint64_t    FsOffset = 0;
  std::array<uint8_t, 32> InodeId;
  uint32_t    AddrFlags = 0;
  std::string Path;
  uint64_t    FileSize = 0;
  uint64_t    DiskOffset = 0;
  TSK_FS_TYPE_ENUM FsType = TSK_FS_TYPE_DETECT;

private:
  std::unique_ptr<ReadSeek> stream;
};