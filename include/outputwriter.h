#pragma once

#include <memory>
#include <string>
#include <vector>

struct OutputChunk;
class BlockSequence;

class OutputWriter {
public:
  virtual ~OutputWriter() {}

  virtual void outputImage(const OutputChunk& c) = 0;

  virtual void outputDirent(const OutputChunk& c) = 0;

  virtual void outputInode(const OutputChunk& c) = 0;

  virtual void outputSearchHit(const std::string& hit) = 0;

  virtual void outputFile(uint64_t size, const std::string& path, const BlockSequence& file) = 0;

  virtual void close() = 0;
};
