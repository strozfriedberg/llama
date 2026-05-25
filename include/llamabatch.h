#pragma once

#include <array>
#include <cstdint>
#include <string>

#include <duckdb.h>

#include "llamaduck.h"

struct RuleRec
{
  static constexpr auto ColNames = {"id",
                                    "name"};

  std::string Id;
  std::string Name;
};

struct RuleMatch {
  static constexpr auto ColNames = {"id",
                                    "path",
                                    "name",
                                    "inode_id"};

  std::string id;
  std::string path;
  std::string name;
  std::array<uint8_t, 32> inode_id;
};

struct SearchHit {
  static constexpr auto ColNames = {"pattern",
                                    "start_offset",
                                    "end_offset",
                                    "rule_id",
                                    "file_hash",
                                    "length"};
  
  std::string pattern;
  uint64_t start_offset;
  uint64_t end_offset;
  std::string rule_id;
  std::array<uint8_t, 32> file_hash;
  uint64_t length;
};

using RuleMatchBatch = DBBatch<RuleMatch>;
using RuleRecBatch = DBBatch<RuleRec>;
