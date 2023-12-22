#pragma once

#include <cstddef>
#include <vector>

class ReadSeek {
public:
  virtual size_t read(size_t len, std::vector<uint8_t>& buf) = 0;

  virtual size_t tellg(size_t pos) const = 0;
  virtual size_t seek(size_t pos) = 0;

  virtual size_t size(void) const = 0;
};
