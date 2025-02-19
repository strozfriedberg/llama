#include "readseek_impl.h"

#include <algorithm>
#include <cstdio>

#include "throw.h"

namespace {
  static std::vector<uint8_t> pdfSig = {0x25, 0x50, 0x44, 0x46, 0x2D}; // %PDF-
}

int64_t ReadSeekBuf::read(size_t len, std::vector<uint8_t>& buf) {
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

bool ReadSeekBuf::isPDF() {
  return std::equal(Buf.begin(), Buf.begin() + pdfSig.size(), pdfSig.begin(), pdfSig.end());
}

//*******************************************************************

ReadSeekFile::ReadSeekFile(std::shared_ptr<FILE> fileptr):
  FilePtr(fileptr), Pos(0), Size(0)
{
  std::fseek(FilePtr.get(), 0, SEEK_END);
  Size = std::ftell(FilePtr.get());
  std::fseek(FilePtr.get(), 0, SEEK_SET);
}

bool ReadSeekFile::isPDF() {
  std::vector<uint8_t> buf;
  read(pdfSig.size(), buf);
  seek(0);
  return (buf == pdfSig);
}

int64_t ReadSeekFile::read(size_t len, std::vector<uint8_t>& buf) {
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

//*******************************************************************

ReadSeekTSK::ReadSeekTSK(const std::shared_ptr<TSK_FS_INFO>& fs, uint64_t inum):
  Fs(fs),
  Inum(inum),
  FilePtr(nullptr),
  Pos(0)
{}

bool ReadSeekTSK::open(void) {
  if (!FilePtr) {
    FilePtr = tsk_fs_file_open_meta(Fs.get(), nullptr, Inum);
    if (!FilePtr) {
      return false;
    }
  }
  return true;
}

void ReadSeekTSK::close(void) {
  if (FilePtr) {
    tsk_fs_file_close(FilePtr);
    FilePtr = nullptr;
  }
}

bool ReadSeekTSK::isPDF() {
  std::vector<uint8_t> buf;
  read(pdfSig.size(), buf);
  seek(0);
  return (buf == pdfSig);
}

int64_t ReadSeekTSK::read(size_t len, std::vector<uint8_t>& buf) {
  if (FilePtr && Pos < size_t(FilePtr->meta->size)) {
    buf.resize(len);
    auto bytesRead = tsk_fs_file_read(FilePtr, Pos, (char*)buf.data(), len, TSK_FS_FILE_READ_FLAG_NONE);
    buf.resize(bytesRead);
    Pos += bytesRead;
    return bytesRead;
  }
  return 0;
}

size_t ReadSeekTSK::seek(size_t pos) {
  if (FilePtr) {
    Pos = std::min(pos, size_t(FilePtr->meta->size));
    return Pos;
  }
  return 0;
}

size_t ReadSeekTSK::size(void) const {
  return FilePtr ? FilePtr->meta->size : 0;
}

