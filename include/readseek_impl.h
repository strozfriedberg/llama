#pragma once

#include "readseek.h"
#include "tsk.h"

#include <memory>

class ReadSeekBuf: public ReadSeek {
public:
  ReadSeekBuf(const std::vector<uint8_t>& buf): Buf(buf), Pos(0) {}
  ReadSeekBuf(const std::string& str) : Buf(str.begin(), str.end()), Pos(0) {}
  virtual ~ReadSeekBuf() {}

  virtual bool open(void) override { return true; }
  virtual void close(void) override {}

  virtual uint64_t getID() const override { return 0; }

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

  virtual bool open(void) override { return true; }
  virtual void close(void) override {}

  virtual uint64_t getID() const override { return 0; }

  virtual int64_t read(size_t len, std::vector<uint8_t>& buf) override;

  virtual size_t tellg() const override;
  virtual size_t seek(size_t pos) override;

  virtual size_t size(void) const override;

private:
  std::shared_ptr<FILE> FilePtr;

  size_t Pos, Size;
};

//*******************************************************************

struct TSK_FS_ATTR;

class ReadSeekTSK: public ReadSeek {
public:
  ReadSeekTSK(const std::shared_ptr<TSK_FS_INFO>& fs, uint64_t inum);
  virtual ~ReadSeekTSK() {}

  virtual bool open(void) override;
  virtual void close(void) override;

  virtual uint64_t getID() const override { return Inum; }

  virtual int64_t read(size_t len, std::vector<uint8_t>& buf) override;

  virtual size_t tellg() const override { return 0; }
  virtual size_t seek(size_t pos) override;

  virtual size_t size(void) const override;

private:
  std::shared_ptr<TSK_FS_INFO> Fs;
  uint64_t Inum;
  TSK_FS_FILE* FilePtr;
  uint64_t Pos;
};

