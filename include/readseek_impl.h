#pragma once

#include "readseek.h"

class ReadSeekBuf: public ReadSeek {
public:
  ReadSeekBuf(const std::vector<uint8_t>& buf): Buf(buf), Pos(0) {}
  virtual ~ReadSeekBuf() {}

  virtual int64_t read(size_t len, std::vector<uint8_t>& buf) override;

  virtual size_t tellg() const override { return Pos; }
  virtual size_t seek(size_t pos) override { return (Pos = (pos < Buf.size() ? pos: Buf.size())); }

  virtual size_t size(void) const override { return Buf.size(); }

private:
  std::vector<uint8_t> Buf;

  size_t Pos;
};

//*******************************************************************

class ReadSeekFile: public ReadSeek {
public:
  ReadSeekFile(std::shared_ptr<FILE> fileptr);
  virtual ~ReadSeekFile() {}

  virtual int64_t read(size_t len, std::vector<uint8_t>& buf) override;

  virtual size_t tellg() const override;
  virtual size_t seek(size_t pos) override;

  virtual size_t size(void) const override;

private:
  std::shared_ptr<FILE> FilePtr;

  size_t Pos, Size;
};

//*******************************************************************

class TSK_FS_ATTR;

class ReadSeekTSK: public ReadSeek {
public:
  ReadSeekTSK(TSK_FS_ATTR* attr); // does not take ownership
  virtual ~ReadSeekTSK() {}

  virtual int64_t read(size_t len, std::vector<uint8_t>& buf) override;

  virtual size_t tellg() const override { return Pos; }
  virtual size_t seek(size_t pos) override;

  virtual size_t size(void) const override;

private:
  TSK_FS_ATTR* TskAttr;

  size_t Pos;
};
