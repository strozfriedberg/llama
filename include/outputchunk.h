#pragma once

#include <cstdint>
#include <string>

struct OutputChunk {
  uint64_t size;
  std::string path;
  std::string data;
};
