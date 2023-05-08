#pragma once

#include <functional>
#include <memory>

#include <tsk/libtsk.h>

#include "jsoncons_wrapper.h"

class TimestampGetter;

class TskFacade {
public:
  virtual ~TskFacade() {}

  virtual std::unique_ptr<TSK_IMG_INFO, void(*)(TSK_IMG_INFO*)> openImg(const char* path) const;

  virtual std::unique_ptr<TSK_FS_INFO, void(*)(TSK_FS_INFO*)> openFS(TSK_IMG_INFO* img, TSK_OFF_T off, TSK_FS_TYPE_ENUM type) const;

  virtual std::unique_ptr<TSK_FS_FILE, void(*)(TSK_FS_FILE*)> openFile(TSK_FS_INFO* fs, TSK_INUM_T inum) const;

  virtual void populateAttrs(TSK_FS_FILE* file) const;

  virtual bool walk(
    TSK_IMG_INFO* info,
    std::function<TSK_FILTER_ENUM(const TSK_VS_INFO*)> vs_cb,
    std::function<TSK_FILTER_ENUM(const TSK_VS_PART_INFO*)> vol_cb,
    std::function<TSK_FILTER_ENUM(TSK_FS_INFO*)> fs_cb,
    std::function<TSK_RETVAL_ENUM(TSK_FS_FILE*, const char*)> file_cb
  );

  virtual jsoncons::json convertImg(const TSK_IMG_INFO& img) const;

  virtual jsoncons::json convertVS(const TSK_VS_INFO& vs) const;

  virtual jsoncons::json convertVol(const TSK_VS_PART_INFO& vol) const;

  virtual jsoncons::json convertFS(const TSK_FS_INFO& fs) const;

  virtual jsoncons::json convertName(const TSK_FS_NAME& name) const;

  virtual jsoncons::json convertMeta(const TSK_FS_META& meta, TimestampGetter& ts) const;

  virtual jsoncons::json convertAttr(const TSK_FS_ATTR& attr) const;

  virtual jsoncons::json convertRun(const TSK_FS_ATTR_RUN& run) const;

  virtual std::unique_ptr<TimestampGetter> makeTimestampGetter(TSK_FS_TYPE_ENUM fstype) const;
};
