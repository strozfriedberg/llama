#pragma once

#include "blocksequence.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include <tsk/libtsk.h>

class FileBlockSequence: public BlockSequence {
public:
  FileBlockSequence(const std::string& path):
    path(path),
    off(0),
    rlen(0),
    bufend(&buf[0])
  {
  }

  virtual ~FileBlockSequence() {}

  virtual Iterator begin() override {
    in.open(path, std::ios::binary);
    return in ? Iterator(this) : end();
  }

protected:
  virtual bool advance() override {
    off += rlen;

    const bool ok = bool(in);

    in.read(reinterpret_cast<char*>(buf), sizeof(buf));
    rlen = in.gcount();
    bufend = buf + rlen;

    return ok;
  }

  virtual std::pair<const uint8_t*, const uint8_t*> cur() const override {
    return std::make_pair(&buf[0], bufend);
  }

  virtual size_t offset() const override {
    return off;
  }

private:
  std::string path;
  std::ifstream in;

  size_t off;
  ssize_t rlen;
  uint8_t buf[8192];
  uint8_t* bufend;
};

class TskBlockSequence: public BlockSequence {
public:
  TskBlockSequence(std::unique_ptr<TSK_FS_FILE, void(*)(TSK_FS_FILE*)> f):
    file(std::move(f)),
    off(0),
    rlen(0)
  {
    // TODO: could this return null?
    const TSK_FS_ATTR* attr = tsk_fs_file_attr_get(file.get());
    size = attr ? attr->size : 0;
  }

  virtual ~TskBlockSequence() {}

protected:
  virtual bool advance() override {
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

  virtual std::pair<const uint8_t*, const uint8_t*> cur() const override {
    return std::make_pair(&buf[0], bufend);
  }

  // the offset of the data at cur().first
  virtual size_t offset() const override {
    return off;
  }

private:
  std::unique_ptr<TSK_FS_FILE, void(*)(TSK_FS_FILE*)> file;
  size_t off;
  ssize_t rlen;
  size_t size;
  uint8_t buf[8192];
  uint8_t* bufend;
};
