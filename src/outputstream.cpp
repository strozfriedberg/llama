#include "outputstream.h"

#include "outputchunk.h"

#include <ostream>

void OutputStream::outputImage(const OutputChunk& c) {
  os << c.data;
}

void OutputStream::outputDirent(const OutputChunk& c) {
  os << c.data;
}

void OutputStream::outputInode(const OutputChunk& c) {
  os << c.data;
}

void OutputStream::outputSearchHit(const std::string&) {
}

void OutputStream::outputFile(uint64_t size, const std::string& path, const BlockSequence&) {
  os << "file contents skipped\t" << path << '\t' << size << std::endl;
}

void OutputStream::close() {
  os.flush();
}
