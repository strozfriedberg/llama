#pragma once

#include "llamaduck.h"

struct SigRec {
  static constexpr auto ColNames = {"Id",
                                    "Name",
                                    "Description"};
  std::string Id;
  std::string Name;
  std::string Description;
};

using SigBatch = DBBatch<SigRec>;

struct FileSigResult {
  static constexpr auto ColNames = {"FileHash",
                                    "SigId"};
  std::string FileHash;
  std::string SigId;
};

using FileSigBatch = DBBatch<FileSigResult>;
