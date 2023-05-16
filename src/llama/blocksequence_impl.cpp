#include "blocksequence_impl.h"

FileBlockSequence::FileBlockSequence(const std::string& path):
  path(path),
  off(0),
  rlen(0),
  bufend(&buf[0])
{
}

BlockSequence::Iterator FileBlockSequence::begin() {
  in.open(path, std::ios::binary);
  return in ? Iterator(this) : end();
}

bool FileBlockSequence::advance() {
  off += rlen;

  const bool ok = bool(in);

  in.read(reinterpret_cast<char*>(buf), sizeof(buf));
  rlen = in.gcount();
  bufend = buf + rlen;

  return ok;
}

std::pair<const uint8_t*, const uint8_t*> FileBlockSequence::cur() const {
  return std::make_pair(&buf[0], bufend);
}

size_t FileBlockSequence::offset() const {
  return off;
}
//***************************************************************************

TskBlockSequence::TskBlockSequence(std::unique_ptr<TSK_FS_FILE, void(*)(TSK_FS_FILE*)> f):
  file(std::move(f)),
  off(0),
  rlen(0)
{
  // TODO: could this return null?
  const TSK_FS_ATTR* attr = tsk_fs_file_attr_get(file.get());
  size = attr ? attr->size : 0;
}

bool TskBlockSequence::advance() {
  // TODO: check that size and final offset comport

  // advance offset by the previous read length
  off += rlen;

  if (off >= size) {
    bufend = &buf[0];
    return false;
  }

  // fill the buffer
  rlen = tsk_fs_file_read(
    file.get(), off,
    reinterpret_cast<char*>(buf), sizeof(buf),
    TSK_FS_FILE_READ_FLAG_NONE
  );

  // mark the end of the data read
  if (rlen < 0) {
    std::cerr << "Error: " << tsk_error_get() << std::endl;
    bufend = &buf[0];
  }
  else {
    bufend = buf + rlen;
  }

  return bufend > &buf[0];
}
