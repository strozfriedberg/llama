// ABOUTME: DuckDB record type for batch dispatch metadata.
// ABOUTME: Tracks batch ID, bucket index, entry count, and total bytes per dispatch.
#pragma once

#include "llamaduck.h"

struct BatchRec {
  static constexpr auto ColNames = {"BatchId",
                                    "BucketIndex",
                                    "NumEntries",
                                    "TotalBytes"};

  uint64_t BatchId;
  uint64_t BucketIndex;
  uint64_t NumEntries;
  uint64_t TotalBytes;
};

using BatchRecBatch = DBBatch<BatchRec>;
