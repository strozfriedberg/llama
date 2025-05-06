#pragma once

#include "readseek.h"

class Entry {
public:
  Entry(std::string name, std::string path, uint64_t addr, std::unique_ptr<ReadSeek> rs) : Name(name), Path(path), Addr(addr), stream(std::move(rs)) {}
  ReadSeek& getStream() { return *stream; }

  std::string Name;
  std::string Path;
  uint64_t Addr;

private:
  std::unique_ptr<ReadSeek> stream;
};