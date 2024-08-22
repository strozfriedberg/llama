#pragma once

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <duckdb.h>

#include "llamaduck.h"
#include "throw.h"

void appendVal(duckdb_appender& appender, const char* s);
void appendVal(duckdb_appender& appender, uint64_t val);

template<typename T>
void append(duckdb_appender& appender, T t) {
  appendVal(appender, t);
}

template<typename T, typename... Args>
void append(duckdb_appender& appender, T t, Args... args) {
  appendVal(appender, t);
  append(appender, args...);
}

struct Dirent : public SchemaType<std::string, std::string, std::string, std::string, std::string, std::string, uint64_t, uint64_t, uint64_t, uint64_t>
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

  size_t size() const { return NumRows; }

  std::vector<char>    Buf; // strings stored in sequence here
  std::vector<uint64_t> OffsetVals; // offsets to strings OR uint64_t values

  uint64_t NumRows = 0;

  static bool createTable(duckdb_connection& dbconn, const std::string& table) {
    duckdb_state state = duckdb_query(dbconn, createQuery<Dirent>(table.c_str()).c_str(), nullptr);
    return state != DuckDBError;
  }

  void add(const Dirent& dent) {
    size_t idSize = dent.Id.size() + 1;
    size_t pathSize = dent.Path.size() + 1;
    size_t nameSize = dent.Name.size() + 1;
    size_t shrtSize = dent.ShortName.size() + 1;
    size_t typeSize = dent.Type.size() + 1;
    size_t flagsSize = dent.Flags.size() + 1;
    size_t startOffset = Buf.size();
    size_t totalSize = idSize + pathSize + nameSize + shrtSize + typeSize + flagsSize;
    Buf.resize(startOffset + totalSize);

    OffsetVals.push_back(startOffset);
    std::copy_n(dent.Id.begin(), idSize, Buf.begin() + OffsetVals.back());
    OffsetVals.push_back(startOffset + idSize);
    std::copy_n(dent.Path.begin(), pathSize, Buf.begin() + OffsetVals.back());
    OffsetVals.push_back(startOffset + idSize + pathSize);
    std::copy_n(dent.Name.begin(), nameSize, Buf.begin() + OffsetVals.back());
    OffsetVals.push_back(startOffset + idSize + pathSize + nameSize);
    std::copy_n(dent.ShortName.begin(), shrtSize, Buf.begin() + OffsetVals.back());
    OffsetVals.push_back(startOffset + idSize + pathSize + nameSize + shrtSize);
    std::copy_n(dent.Type.begin(), typeSize, Buf.begin() + OffsetVals.back());
    OffsetVals.push_back(startOffset + idSize + pathSize + nameSize + shrtSize + typeSize);
    std::copy_n(dent.Flags.begin(), flagsSize, Buf.begin() + OffsetVals.back());
    OffsetVals.push_back(dent.MetaAddr);
    OffsetVals.push_back(dent.ParentAddr);
    OffsetVals.push_back(dent.MetaSeq);
    OffsetVals.push_back(dent.ParentSeq);
    ++NumRows;
  }

  void copyToDB(duckdb_appender& appender) {
    duckdb_state state;
    for (unsigned int i = 0; i + 9 < OffsetVals.size(); i += 10) {
      append(appender,
             Buf.data() + OffsetVals[i],
             Buf.data() + OffsetVals[i + 1],
             Buf.data() + OffsetVals[i + 2],
             Buf.data() + OffsetVals[i + 3],
             Buf.data() + OffsetVals[i + 4],
             Buf.data() + OffsetVals[i + 5],
             OffsetVals[i + 6],
             OffsetVals[i + 7],
             OffsetVals[i + 8],
             OffsetVals[i + 9]
      );
      state = duckdb_appender_end_row(appender);
      THROW_IF(state == DuckDBError, "Failed call to end_row");
    }
    Buf.clear();
    OffsetVals.clear();
    NumRows = 0;
  }
};

