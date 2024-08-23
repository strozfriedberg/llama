#pragma once

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <duckdb.h>

#include "llamaduck.h"
#include "throw.h"


//struct Dirent : public SchemaType<const char*, const char*, const char*, const char*, const char*, const char*, uint64_t, uint64_t, uint64_t, uint64_t>
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

struct DuckDirent : public SchemaType<Dirent, const char*, const char*, const char*, const char*, const char*, const char*, uint64_t, uint64_t, uint64_t, uint64_t>
{
  DuckDirent(): SchemaType() {}
  DuckDirent(const Dirent& base): SchemaType(base) {}
};

class DirentBatch : public DuckBatch {
public:

  void add(const Dirent& dent) {
    size_t startOffset = Buf.size();
    size_t totalSize = totalStringSize(dent.Id, dent.Path, dent.Name, dent.ShortName, dent.Type, dent.Flags);
    Buf.resize(startOffset + totalSize);

    addStrings(*this, startOffset, dent.Id, dent.Path, dent.Name, dent.ShortName, dent.Type, dent.Flags);
 
    OffsetVals.push_back(dent.MetaAddr);
    OffsetVals.push_back(dent.ParentAddr);
    OffsetVals.push_back(dent.MetaSeq);
    OffsetVals.push_back(dent.ParentSeq);
    ++NumRows;
  }

  void copyToDB(duckdb_appender& appender) {
    duckdb_state state;
    for (unsigned int i = 0; i + (DuckDirent::NumCols - 1) < OffsetVals.size(); i += DuckDirent::NumCols) {

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
    DuckBatch::clear();
  }
};

