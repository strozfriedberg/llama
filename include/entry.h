#pragma once

#include "readseek.h"

class Entry {
public:
  Entry(uint64_t addr, std::unique_ptr<ReadSeek> rs) : Addr(addr), stream(std::move(rs)) {}
  ReadSeek& getStream() { return *stream; }

  uint64_t Addr;
  std::string EvidenceFile;
  uint32_t    FsIndex = 0;
  uint64_t    FsOffset = 0;
  uint32_t    AddrFlags = 0;
  std::string Path;
  uint64_t    FileSize = 0;

private:
  std::unique_ptr<ReadSeek> stream;
};