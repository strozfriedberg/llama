#pragma once

#include <array>
#include <cstdint>

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
  std::array<uint8_t, 32> FileHash{};
  std::string SigId;
};

using FileSigBatch = DBBatch<FileSigResult>;
