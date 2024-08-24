#include "duckinode.h"

void InodeBatch::add(const Inode& inode) {
  size_t offset = Buf.size();
  size_t totalSize = totalStringSize(inode.Id, inode.Type, inode.Flags, inode.LinkTarget, inode.Created, inode.Accessed, inode.Modified, inode.Metadata);
  Buf.resize(offset + totalSize);

  offset = addStrings(*this, offset, inode.Id, inode.Type, inode.Flags);
  OffsetVals.push_back(inode.Addr);
  OffsetVals.push_back(inode.FsOffset);
  OffsetVals.push_back(inode.Filesize);
  OffsetVals.push_back(inode.Uid);
  OffsetVals.push_back(inode.Gid);
  offset = addStrings(*this, offset, inode.LinkTarget);
  OffsetVals.push_back(inode.NumLinks);
  OffsetVals.push_back(inode.SeqNum);
  addStrings(*this, offset, inode.Created, inode.Accessed, inode.Modified, inode.Metadata);
  ++NumRows;
}

unsigned int InodeBatch::copyToDB(duckdb_appender& appender) {
  unsigned int numRows = 0;
  duckdb_state state;
  unsigned int i = 0;
  while (i + (DuckInode::NumCols - 1) < OffsetVals.size()) {
    append(appender,
            Buf.data() + OffsetVals[i++],
            Buf.data() + OffsetVals[i++],
            Buf.data() + OffsetVals[i++],
            OffsetVals[i++],
            OffsetVals[i++],
            OffsetVals[i++],
            OffsetVals[i++],
            OffsetVals[i++],
            Buf.data() + OffsetVals[i++],
            OffsetVals[i++],
            OffsetVals[i++],
            Buf.data() + OffsetVals[i++],
            Buf.data() + OffsetVals[i++],
            Buf.data() + OffsetVals[i++],
            Buf.data() + OffsetVals[i++]
    );
    state = duckdb_appender_end_row(appender);
    THROW_IF(state == DuckDBError, "Failed call to end_row");
    ++numRows;
  }
  DuckBatch::clear();
  return numRows;
}

