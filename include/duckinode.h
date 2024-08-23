#pragma once

#include "inode.h"
#include "llamaduck.h"

struct DuckInode : public SchemaType<Inode,
                                      const char*,
                                      const char*,
                                      const char*,
                                      uint64_t,
                                      uint64_t,
                                      uint64_t,
                                      const char*,
                                      uint64_t,
                                      uint64_t,
                                      const char*,
                                      const char*,
                                      const char*,
                                      const char*>
{};

class InodeBatch : public DuckBatch {
public:
  void add(const Inode& inode) {
    size_t offset = Buf.size();
    size_t totalSize = totalStringSize(inode.Id, inode.Type, inode.Flags, inode.LinkTarget, inode.Created, inode.Accessed, inode.Modified, inode.Metadata);
    Buf.resize(offset + totalSize);

    offset = addStrings(*this, offset, inode.Id, inode.Type, inode.Flags);
    OffsetVals.push_back(inode.Addr);
    OffsetVals.push_back(inode.Uid);
    OffsetVals.push_back(inode.Gid);
    offset = addStrings(*this, offset, inode.LinkTarget);
    OffsetVals.push_back(inode.NumLinks);
    OffsetVals.push_back(inode.SeqNum);
    addStrings(*this, offset, inode.Created, inode.Accessed, inode.Modified, inode.Metadata);
    ++NumRows;
  }

  unsigned int copyToDB(duckdb_appender& appender) {
    unsigned int numRows = 0;
    duckdb_state state;
    for (unsigned int i = 0; i + (DuckDirent::NumCols - 1) < OffsetVals.size(); i += DuckDirent::NumCols) {
      append(appender,
              Buf.data() + OffsetVals[i],
              Buf.data() + OffsetVals[i + 1],
              Buf.data() + OffsetVals[i + 2],
              OffsetVals[i + 3],
              OffsetVals[i + 4],
              OffsetVals[i + 5],
              Buf.data() + OffsetVals[i + 6],
              OffsetVals[i + 7],
              OffsetVals[i + 8],
              Buf.data() + OffsetVals[i + 9],
              Buf.data() + OffsetVals[i + 10],
              Buf.data() + OffsetVals[i + 11],
              Buf.data() + OffsetVals[i + 12]
      );
      state = duckdb_appender_end_row(appender);
      THROW_IF(state == DuckDBError, "Failed call to end_row");
      ++numRows;
    }
    DuckBatch::clear();
    return numRows;
  }
};
