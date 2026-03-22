// ABOUTME: DuckDB record struct for the evidence exception log table
// ABOUTME: Stores context about evidence files that could not be read or processed

#pragma once

#include "llamaduck.h"

struct ExceptionRecord {
  static constexpr auto ColNames = {"evidence_file",
                                    "fs_index",
                                    "fs_offset",
                                    "addr",
                                    "addr_flags",
                                    "path",
                                    "file_size",
                                    "operation",
                                    "timestamp",
                                    "message"};

  std::string EvidenceFile;
  uint64_t    FsIndex;
  uint64_t    FsOffset;
  uint64_t    Addr;
  std::string AddrFlags;
  std::string Path;
  uint64_t    FileSize;
  std::string Operation;
  std::string Timestamp;
  std::string Message;
};

using ExceptionBatch = DBBatch<ExceptionRecord>;
