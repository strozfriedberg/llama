#pragma once

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <duckdb.h>

#include "throw.h"

struct Dirent {
  std::string Path;
  std::string Name;
  std::string Shrt_name;

  std::string Type;
  std::string Flags;

  uint64_t Meta_addr;
  uint64_t Par_addr;
  uint64_t Meta_seq;
  uint64_t Par_seq;

  Dirent(const std::string& p = "",
         const std::string& n = "",
         const std::string& shrt = "",
         const std::string& type = "",
         const std::string& flags = "",
         uint64_t meta = 0,
         uint64_t parent = 0,
         uint64_t meta_seq = 0,
         uint64_t par_seq = 0
  ):
    Path(p), Name(n), Shrt_name(shrt), Type(type), Flags(flags),
    Meta_addr(meta), Par_addr(parent), Meta_seq(meta_seq), Par_seq(par_seq) {}
};

struct DirentBatch {
  struct StrOffsets {
    size_t PathOffset,
           NameOffset,
           ShrtOffset,
           TypeOffset,
           FlagsOffset;
  };

  struct Uint64Vals {
    uint64_t Meta_addr,
             Par_addr,
             Meta_seq,
             Par_seq;
  };

  size_t size() const { return Offsets.size(); }

  std::vector<char> Buf;
  std::vector<StrOffsets> Offsets;
  std::vector<Uint64Vals> Nums;

  static bool createTable(duckdb_connection& dbconn, const std::string& table) {
    std::string query = "CREATE TABLE " + table + " (path VARCHAR, name VARCHAR, shrt_name VARCHAR, type VARCHAR, flags VARCHAR, meta_addr UBIGINT, par_addr UBIGINT, meta_seq UBIGINT, par_seq UBIGINT);";
    return DuckDBSuccess == duckdb_query(dbconn, query.c_str(), nullptr);
  }

  void add(const Dirent& dent) {
    size_t pathSize = dent.Path.size() + 1;
    size_t nameSize = dent.Name.size() + 1;
    size_t shrtSize = dent.Shrt_name.size() + 1;
    size_t typeSize = dent.Type.size() + 1;
    size_t flagsSize = dent.Flags.size() + 1;
    size_t startOffset = Buf.size();
    size_t totalSize = pathSize + nameSize + shrtSize + typeSize + flagsSize;
    Buf.resize(startOffset + totalSize);
    StrOffsets offsets{startOffset,
                       startOffset + pathSize,
                       startOffset + pathSize + nameSize,
                       startOffset + pathSize + nameSize + shrtSize,
                       startOffset + pathSize + nameSize + shrtSize + typeSize};

    std::copy_n(dent.Path.begin(), pathSize, Buf.begin() + offsets.PathOffset);
    std::copy_n(dent.Name.begin(), nameSize, Buf.begin() + offsets.NameOffset);
    std::copy_n(dent.Shrt_name.begin(), shrtSize, Buf.begin() + offsets.ShrtOffset);
    std::copy_n(dent.Type.begin(), typeSize, Buf.begin() + offsets.TypeOffset);
    std::copy_n(dent.Flags.begin(), flagsSize, Buf.begin() + offsets.FlagsOffset);

    Offsets.push_back(offsets);
    Nums.push_back(Uint64Vals{dent.Meta_addr, dent.Par_addr, dent.Meta_seq, dent.Par_seq});
  }

  void copyToDB(duckdb_appender& appender) {
    duckdb_state state;
    for (uint32_t i = 0; i < Offsets.size(); ++i) {
      auto& offsets(Offsets[i]);
      state = duckdb_append_varchar(appender, Buf.data() + offsets.PathOffset);
      THROW_IF(state == DuckDBError, "Failed to append path");
      state = duckdb_append_varchar(appender, Buf.data() + offsets.NameOffset);
      THROW_IF(state == DuckDBError, "Failed to append name");
      state = duckdb_append_varchar(appender, Buf.data() + offsets.ShrtOffset);
      THROW_IF(state == DuckDBError, "Failed to append shrt_name");
      state = duckdb_append_varchar(appender, Buf.data() + offsets.TypeOffset);
      THROW_IF(state == DuckDBError, "Failed to append type");
      state = duckdb_append_varchar(appender, Buf.data() + offsets.FlagsOffset);
      THROW_IF(state == DuckDBError, "Failed to append flags");

      auto& nums(Nums[i]);
      state = duckdb_append_uint64(appender, nums.Meta_addr);
      THROW_IF(state == DuckDBError, "Failed to append meta_addr");
      state = duckdb_append_uint64(appender, nums.Par_addr);
      THROW_IF(state == DuckDBError, "Failed to append par_addr");
      state = duckdb_append_uint64(appender, nums.Meta_seq);
      THROW_IF(state == DuckDBError, "Failed to append meta_seq");
      state = duckdb_append_uint64(appender, nums.Par_seq);
      THROW_IF(state == DuckDBError, "Failed to append par_seq");

      state = duckdb_appender_end_row(appender);
      THROW_IF(state == DuckDBError, "Failed call to end_row");
    }
  }
};
