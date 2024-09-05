#pragma once

#include "llamaduck.h"

struct HashRec {

  static constexpr auto ColNames = {"MetaAddr",
                                    "MD5",
                                    "SHA1",
                                    "SHA256",
                                    "Blake3",
                                    "Ssdeep"};

  uint64_t MetaAddr;

  std::string MD5;
  std::string SHA1;
  std::string SHA256;
  std::string Blake3;
  std::string Ssdeep;
};

struct DuckHashRec : public SchemaType<HashRec,
                                      uint64_t,
                                      const char*,
                                      const char*,
                                      const char*,
                                      const char*,
                                      const char*>
{};

class HashBatch : public DuckBatch {
public:
  void add(const HashRec& hashes) { 
    size_t offset = Buf.size();
    size_t totalSize = totalStringSize(hashes.MD5, hashes.SHA1, hashes.SHA256, hashes.Blake3, hashes.Ssdeep);
    Buf.resize(offset + totalSize);

    OffsetVals.push_back(hashes.MetaAddr);
    addStrings(*this, offset, hashes.MD5, hashes.SHA1, hashes.SHA256, hashes.Blake3, hashes.Ssdeep);
    ++NumRows;
  }

  unsigned int copyToDB(duckdb_appender& appender) {
    unsigned int numRows = 0;
    duckdb_state state;
    for (unsigned int i = 0; i + (DuckHashRec::NumCols - 1) < OffsetVals.size(); i += DuckHashRec::NumCols) {
      append(appender,
             OffsetVals[i],
             Buf.data() + OffsetVals[i + 1],
             Buf.data() + OffsetVals[i + 2],
             Buf.data() + OffsetVals[i + 3],
             Buf.data() + OffsetVals[i + 4],
             Buf.data() + OffsetVals[i + 5]
      );
      state = duckdb_appender_end_row(appender);
      THROW_IF(state == DuckDBError, "Failed call to end_row");
      ++numRows;
    }
    DuckBatch::clear();
    return numRows;
  }
};

