#pragma once

#include <functional>

#include <tsk/libtsk.h>

#include "tskfacade.h"
#include "jsoncons_wrapper.h"
#include "inodeandblocktracker.h"

namespace TskReaderHelper {
  void handleRuns(
    const TSK_FS_ATTR& a,
    uint64_t fsOffset,
    uint64_t blockSize,
    uint64_t inum,
    TskFacade& tsk,
    InodeAndBlockTracker& tracker,
    void (InodeAndBlockTracker::*markDataRun)(uint64_t, uint64_t, uint64_t),
    jsoncons::json& jnrd_runs
  );

  void handleAttrs(
    const TSK_FS_META& meta,
    uint64_t fsOffset,
    uint64_t blockSize,
    uint64_t inum,
    TskFacade& tsk,
    InodeAndBlockTracker& tracker,
    jsoncons::json& jattrs
  );
}
