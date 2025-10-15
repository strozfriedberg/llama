#include "util.h"
#include "readseek.h"

#include <random>

namespace {
  const std::vector<uint8_t> pdfSig = {0x25, 0x50, 0x44, 0x46, 0x2D}; // %PDF-
}

std::string randomNumString() {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 999999999);
  return std::to_string(dis(gen));
}

bool isPDF(ReadSeek& rs) {
  std::vector<uint8_t> buf;
  rs.read(pdfSig.size(), buf);
  rs.seek(0);
  return (buf == pdfSig);
}
