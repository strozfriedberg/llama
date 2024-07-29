#pragma once

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <duckdb.h>

#include "throw.h"

struct Dirent {
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

  Dirent(const std::string& p = "",
         const std::string& n = "",
         const std::string& shrt = "",
         const std::string& type = "",
         const std::string& flags = "",
         uint64_t meta = 0,
         uint64_t parent = 0,
         uint64_t metaSeq = 0,
         uint64_t parentSeq = 0
  ):
    Path(p), Name(n), ShortName(shrt), Type(type), Flags(flags),
    MetaAddr(meta), ParentAddr(parent), MetaSeq(metaSeq), ParentSeq(parentSeq) {}

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

struct DirentBatch {
  struct StrOffsets {
    size_t PathOffset,
           NameOffset,
           ShortOffset,
           TypeOffset,
           FlagsOffset;
  };

  struct Uint64Vals {
    uint64_t MetaAddr,
             ParentAddr,
             MetaSeq,
             ParentSeq;
  };

  size_t size() const { return Offsets.size(); }

  std::vector<char> Buf;
  std::vector<StrOffsets> Offsets;
  std::vector<Uint64Vals> Nums;

  static bool createTable(duckdb_connection& dbconn, const std::string& table) {
    std::string query = "CREATE TABLE " + table + " (path VARCHAR, name VARCHAR, short_name VARCHAR, type VARCHAR, flags VARCHAR, meta_addr UBIGINT, parent_addr UBIGINT, meta_seq UBIGINT, parent_seq UBIGINT);";
    return DuckDBSuccess == duckdb_query(dbconn, query.c_str(), nullptr);
  }

  void add(const Dirent& dent) {
    size_t pathSize = dent.Path.size() + 1;
    size_t nameSize = dent.Name.size() + 1;
    size_t shrtSize = dent.ShortName.size() + 1;
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
    std::copy_n(dent.ShortName.begin(), shrtSize, Buf.begin() + offsets.ShortOffset);
    std::copy_n(dent.Type.begin(), typeSize, Buf.begin() + offsets.TypeOffset);
    std::copy_n(dent.Flags.begin(), flagsSize, Buf.begin() + offsets.FlagsOffset);

    Offsets.push_back(offsets);
    Nums.push_back(Uint64Vals{dent.MetaAddr, dent.ParentAddr, dent.MetaSeq, dent.ParentSeq});
  }

  void copyToDB(duckdb_appender& appender) {
    duckdb_state state;
    for (uint32_t i = 0; i < Offsets.size(); ++i) {
      auto& offsets(Offsets[i]);
      state = duckdb_append_varchar(appender, Buf.data() + offsets.PathOffset);
      THROW_IF(state == DuckDBError, "Failed to append path");
      state = duckdb_append_varchar(appender, Buf.data() + offsets.NameOffset);
      THROW_IF(state == DuckDBError, "Failed to append name");
      state = duckdb_append_varchar(appender, Buf.data() + offsets.ShortOffset);
      THROW_IF(state == DuckDBError, "Failed to append shrt_name");
      state = duckdb_append_varchar(appender, Buf.data() + offsets.TypeOffset);
      THROW_IF(state == DuckDBError, "Failed to append type");
      state = duckdb_append_varchar(appender, Buf.data() + offsets.FlagsOffset);
      THROW_IF(state == DuckDBError, "Failed to append flags");

      auto& nums(Nums[i]);
      state = duckdb_append_uint64(appender, nums.MetaAddr);
      THROW_IF(state == DuckDBError, "Failed to append meta_addr");
      state = duckdb_append_uint64(appender, nums.ParentAddr);
      THROW_IF(state == DuckDBError, "Failed to append parent_addr");
      state = duckdb_append_uint64(appender, nums.MetaSeq);
      THROW_IF(state == DuckDBError, "Failed to append meta_seq");
      state = duckdb_append_uint64(appender, nums.ParentSeq);
      THROW_IF(state == DuckDBError, "Failed to append parent_seq");

      state = duckdb_appender_end_row(appender);
      THROW_IF(state == DuckDBError, "Failed call to end_row");
    }
    Buf.clear();
    Offsets.clear();
    Nums.clear();
  }
};
