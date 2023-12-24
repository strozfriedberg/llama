#include "readseek_impl.h"

#include <algorithm>

ssize_t ReadSeekBuf::read(size_t len, std::vector<uint8_t>& buf) {
  if (Pos >= Buf.size()) {
    return 0;
  }
  size_t toRead = std::min(len, Buf.size() - Pos);
  buf.resize(toRead);
  for (size_t i = 0, cur = Pos; i < toRead; ++i, ++cur) {
    buf[i] = Buf[cur];
  }
  Pos += toRead;
  return toRead;
}
