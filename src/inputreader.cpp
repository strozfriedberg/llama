#include "inputreader.h"

#include "dirreader.h"
#include "tskreader.h"
#ifdef __linux__
#include "posixreader.h"
#endif

std::shared_ptr<InputReader>
InputReader::createTSK(const std::string& imgName) {
  auto ret = std::make_shared<TskReader>(imgName);
  if (!ret->open()) {
    throw std::runtime_error("Couldn't open image " + imgName);
  }
  return std::static_pointer_cast<InputReader>(ret);
}

std::shared_ptr<InputReader>
InputReader::createDir(const std::string & dir) {
// TODO: maybe throw on failure?
  auto ret = std::make_shared<DirReader>(dir);
  return std::static_pointer_cast<InputReader>(ret);
}

#ifdef __linux__
std::shared_ptr<InputReader>
InputReader::createPosix(const std::string& mountpoint) {
  auto ret = std::make_shared<PosixReader>(mountpoint);
  return std::static_pointer_cast<InputReader>(ret);
}
#endif
