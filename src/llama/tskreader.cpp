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
