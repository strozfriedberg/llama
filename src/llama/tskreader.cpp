#include "tskreader.h"

TskReader::TskReader(const std::string& imgPath):
  ImgPath(imgPath),
  Img(nullptr, nullptr),
  Input(),
  Output(),
  Tsk(new TskFacade),
  Asm(),
  Tsg(nullptr),
  Tracker(new InodeAndBlockTrackerImpl()),
  RecHasher(),
  Dirents(RecHasher)
{
}

bool TskReader::open() {
  return bool(Img = Tsk->openImg(ImgPath.c_str()));
}
