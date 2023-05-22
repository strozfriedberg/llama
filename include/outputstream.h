#pragma once

#include <iosfwd>

#include "outputwriter.h"

class OutputStream: public OutputWriter {
public:
  OutputStream(std::ostream& os): os(os) {}

  virtual void outputImage(const OutputChunk& c) override;

  virtual void outputDirent(const OutputChunk& c) override;

  virtual void outputInode(const OutputChunk& c) override;

  virtual void outputSearchHit(const std::string& hit) override;

  virtual void outputFile(uint64_t size, const std::string& path, const BlockSequence& file) override;

  virtual void close() override;

private:
  std::ostream& os;
};
