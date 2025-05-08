#pragma once

#include "readseek.h"

class Entry {
public:
  Entry(uint64_t addr, std::unique_ptr<ReadSeek> rs) : Addr(addr), stream(std::move(rs)) {}
  ReadSeek& getStream() { return *stream; }

  uint64_t Addr;

  // TODO: Add an attribute enum to signify different stream types
  // E.g., alternate data streams, extracted PDF text, archive files

private:
  std::unique_ptr<ReadSeek> stream;
};