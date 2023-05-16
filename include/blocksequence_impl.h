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
  FileBlockSequence(const std::string& path);

  virtual ~FileBlockSequence() {}

  virtual Iterator begin() override;

protected:
  virtual bool advance() override;

  virtual std::pair<const uint8_t*, const uint8_t*> cur() const override;

  virtual size_t offset() const override;

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
  TskBlockSequence(std::unique_ptr<TSK_FS_FILE, void(*)(TSK_FS_FILE*)> f);

  virtual ~TskBlockSequence() {}

protected:
  virtual bool advance() override;

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
