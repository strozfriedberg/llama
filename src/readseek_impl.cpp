#include "readseek_impl.h"

#include <algorithm>
#include <cstdio>

#include "throw.h"

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

//*******************************************************************

ReadSeekFile::ReadSeekFile(std::shared_ptr<FILE> fileptr):
  FilePtr(fileptr), Pos(0), Size(0)
{
  std::fseek(FilePtr.get(), 0, SEEK_END);
  Size = std::ftell(FilePtr.get());
  std::fseek(FilePtr.get(), 0, SEEK_SET);
}

ssize_t ReadSeekFile::read(size_t len, std::vector<uint8_t>& buf) {
  if (std::feof(FilePtr.get()) || len == 0) {
    return 0;
  }
  buf.resize(len);
  size_t ret = std::fread(buf.data(), 1, len, FilePtr.get());
  Pos = std::min(Size, Pos + ret);
  THROW_IF(ret < len && std::ferror(FilePtr.get()), "call to fread() had error");
  return ret;
};

size_t ReadSeekFile::tellg() const {
  return Pos;
}

size_t ReadSeekFile::seek(size_t pos) {
  Pos = std::min(pos, Size);
  std::fseek(FilePtr.get(), Pos, SEEK_SET);
  return Pos;
};

size_t ReadSeekFile::size(void) const {
  return Size;
}
