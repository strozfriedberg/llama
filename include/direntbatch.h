#pragma once

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

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
                                    "ParentSeq"};

  std::string Id;
  std::string Path;
  std::string Name;
  std::string ShortName;

  std::string Type;
  std::string Flags;

  uint64_t MetaAddr;
  uint64_t ParentAddr;
  uint64_t MetaSeq;
  uint64_t ParentSeq;

  bool operator==(const Dirent& other) const {
    return Id == other.Id &&
           Path == other.Path &&
           Name == other.Name &&
           ShortName == other.ShortName &&
           Type == other.Type &&
           Flags == other.Flags &&
           MetaAddr == other.MetaAddr &&
           ParentAddr == other.ParentAddr &&
           MetaSeq == other.MetaSeq &&
           ParentSeq == other.ParentSeq;
  }
};

using DirentBatch = DBBatch<Dirent>;

