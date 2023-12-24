#pragma once

#include "readseek.h"

class ReadSeekBuf: public ReadSeek {
public:
  ReadSeekBuf(const std::vector<uint8_t>& buf): Buf(buf), Pos(0) {}
  virtual ~ReadSeekBuf() {}

  virtual ssize_t read(size_t len, std::vector<uint8_t>& buf) override;

  virtual size_t tellg() const override { return Pos; }
  virtual size_t seek(size_t pos) override { return (Pos = (pos < Buf.size() ? pos: Buf.size())); }

  virtual size_t size(void) const override { return Buf.size(); }

private:
  std::vector<uint8_t> Buf;

  size_t Pos;
};
