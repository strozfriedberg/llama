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

bool TskReader::startReading() {
  Asm.addImage(Tsk->convertImg(*Img));

  // tell TskAuto to start giving files to processFile
  // std::cerr << "Image is " << getImageSize() << " bytes in size" << std::endl;
  const bool ret = Tsk->walk(
    Img.get(),
    [this](const TSK_VS_INFO* vs_info) { return filterVs(vs_info); },
    [this](const TSK_VS_PART_INFO* vs_part) { return filterVol(vs_part); },
    [this](TSK_FS_INFO* fs_info) { return filterFs(fs_info); },
    [this](TSK_FS_FILE* fs_file, const char* path) { return processFile(fs_file, path); }
  );

  if (ret) {
    // wrap up the walk
    while (!Dirents.empty()) {
      Output->outputDirent(Dirents.pop());
    }
    Output->outputImage(Asm.dump());

    // teardown
    Input->flush();
  }
  return ret;
}

TSK_FILTER_ENUM TskReader::filterVs(const TSK_VS_INFO* vs_info) {
  Asm.addVolumeSystem(Tsk->convertVS(*vs_info));
  return TSK_FILTER_CONT;
}

TSK_FILTER_ENUM TskReader::filterVol(const TSK_VS_PART_INFO* vs_part) {
  Asm.addVolume(Tsk->convertVol(*vs_part));
  return TSK_FILTER_CONT;
}
