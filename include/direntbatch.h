#pragma once

#include <array>
#include <cstdint>
#include <string>

#include <duckdb.h>

#include "llamaduck.h"

struct Dirent
{
  static constexpr auto ColNames = {"Id",
                                    "Path",
                                    "Name",
                                    "ShortName",
                                    "Type",
                                    "Flags",
                                    "MetaAddr",
                                    "ParentAddr",
                                    "MetaSeq",
                                    "ParentSeq",
                                    "MetaId",
                                    "ParentId"};

  std::array<uint8_t, 32> Id{};
  std::string Path;
  std::string Name;
  std::string ShortName;

  std::string Type;
  std::string Flags;

  uint64_t MetaAddr;
  uint64_t ParentAddr;
  uint64_t MetaSeq;
  uint64_t ParentSeq;

  std::array<uint8_t, 32> MetaId{};
  std::array<uint8_t, 32> ParentId{};
};

using DirentBatch = DBBatch<Dirent>;

