#include "tskfacade.h"

#include "tskautowrapper.h"
#include "tskconversion.h"
#include "tsktimestamps.h"
#include "util.h"

std::unique_ptr<TSK_IMG_INFO, void(*)(TSK_IMG_INFO*)> TskFacade::openImg(const char* path) const {
  return make_unique_del(
    tsk_img_open_utf8(1, &path, TSK_IMG_TYPE_DETECT, 0),
    tsk_img_close
  );
}

std::unique_ptr<TSK_FS_INFO, void(*)(TSK_FS_INFO*)> TskFacade::openFS(TSK_IMG_INFO* img, TSK_OFF_T off, TSK_FS_TYPE_ENUM type) const {
  return make_unique_del(tsk_fs_open_img(img, off, type), tsk_fs_close);
}

std::unique_ptr<TSK_FS_FILE, void(*)(TSK_FS_FILE*)> TskFacade::openFile(TSK_FS_INFO* fs, TSK_INUM_T inum) const {
  return make_unique_del(
    tsk_fs_file_open_meta(fs, nullptr, inum),
    tsk_fs_file_close
  );
}

void TskFacade::populateAttrs(TSK_FS_FILE* file) const {
  // force attrs to be populated
  tsk_fs_file_attr_get_idx(file, 0);
}

bool TskFacade::walk(
  TSK_IMG_INFO* info,
  std::function<TSK_FILTER_ENUM(const TSK_VS_INFO*)> vs_cb,
  std::function<TSK_FILTER_ENUM(const TSK_VS_PART_INFO*)> vol_cb,
  std::function<TSK_FILTER_ENUM(TSK_FS_INFO*)> fs_cb,
  std::function<TSK_RETVAL_ENUM(TSK_FS_FILE*, const char*)> file_cb
)
{
  return TskAutoWrapper(info, vs_cb, vol_cb, fs_cb, file_cb).findFilesInImg() == 0;
}

jsoncons::json TskFacade::convertImg(const TSK_IMG_INFO& img) const {
  return TskUtils::convertImg(img);
}

jsoncons::json TskFacade::convertVS(const TSK_VS_INFO& vs) const {
  return TskUtils::convertVS(vs);
}

jsoncons::json TskFacade::convertVol(const TSK_VS_PART_INFO& vol) const {
  return TskUtils::convertVol(vol); 
}

jsoncons::json TskFacade::convertFS(const TSK_FS_INFO& fs) const {
  return TskUtils::convertFS(fs);
}

jsoncons::json TskFacade::convertName(const TSK_FS_NAME& name) const {
  return TskUtils::convertName(name);
}

jsoncons::json TskFacade::convertMeta(const TSK_FS_META& meta, TimestampGetter& ts) const {
  return TskUtils::convertMeta(meta, ts);
}

jsoncons::json TskFacade::convertAttr(const TSK_FS_ATTR& attr) const {
  return TskUtils::convertAttr(attr);
}

jsoncons::json TskFacade::convertRun(const TSK_FS_ATTR_RUN& run) const {
  return TskUtils::convertRun(run);
}

std::unique_ptr<TimestampGetter> TskFacade::makeTimestampGetter(TSK_FS_TYPE_ENUM fstype) const {
  return TskUtils::makeTimestampGetter(fstype);
}
