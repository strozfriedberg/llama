#pragma once

#include "readseek.h"

class Entry {
public:
  Entry(std::unique_ptr<ReadSeek> rs) : stream(std::move(rs)) {}
  ReadSeek& getStream() { return *stream; }

  std::string Id;
  std::string Name;
  std::string Path;

private:
  std::unique_ptr<ReadSeek> stream;
};