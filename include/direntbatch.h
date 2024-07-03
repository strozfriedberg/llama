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
};

struct DirentBatch {
  size_t size() const { return Offsets.size(); }

  std::vector<char> Buf;
  std::vector<std::pair<size_t, size_t>> Offsets;

  static bool createTable(duckdb_connection& dbconn, const std::string& table) {
    std::string query = "CREATE TABLE " + table + " (path VARCHAR, name VARCHAR);";
    return DuckDBSuccess == duckdb_query(dbconn, query.c_str(), nullptr);
  }

  void add(const Dirent& dent) {
    size_t pathSize = dent.Path.size();
    size_t totalSize = pathSize + dent.Name.size();
    size_t offset = Buf.size();
    Buf.resize(offset + totalSize + 1);
    Offsets.push_back({offset, offset + pathSize});
    std::copy(dent.Path.begin(), dent.Path.end(), Buf.begin() + Offsets.back().first);
    std::copy(dent.Name.begin(), dent.Name.end(), Buf.begin() + Offsets.back().second);
    Buf[offset + totalSize] = '\0';
  }

  void copyToDB(duckdb_appender& appender) {
    duckdb_state state;
    for (auto& offsets: Offsets) {
      state = duckdb_append_varchar_length(appender, Buf.data() + offsets.first, offsets.second - offsets.first);
      THROW_IF(state == DuckDBError, "Failed to append path");
      state = duckdb_append_varchar(appender, Buf.data() + offsets.second);
      THROW_IF(state == DuckDBError, "Failed to append name");
      state = duckdb_appender_end_row(appender);
      THROW_IF(state == DuckDBError, "Failed call to end_row");
    }
  }
};
