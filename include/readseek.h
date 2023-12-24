#pragma once

#include <cstddef>
#include <vector>

class ReadSeek {
public:
  virtual ~ReadSeek() {}

  virtual ssize_t read(size_t len, std::vector<uint8_t>& buf) = 0;

  virtual size_t tellg() const = 0;
  virtual size_t seek(size_t pos) = 0;

  virtual size_t size(void) const = 0;
};
