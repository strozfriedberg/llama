#include "duckinode.h"

#include <cassert>

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
    /*assert(DuckInode::colIndex("Id") == 0);
    assert(DuckInode::colIndex("Type") == 1);
    assert(DuckInode::colIndex("Flags") == 2);
    assert(DuckInode::colIndex("Addr") == 3);
    assert(DuckInode::colIndex("FsOffset") == 4);
    assert(DuckInode::colIndex("Filesize") == 5);
    assert(DuckInode::colIndex("Uid") == 6);
    assert(DuckInode::colIndex("Gid") == 7);
    assert(DuckInode::colIndex("LinkTarget") == 8);
    assert(DuckInode::colIndex("NumLinks") == 9);
    assert(DuckInode::colIndex("SeqNum") == 10);
    assert(DuckInode::colIndex("Created") == 11);
    assert(DuckInode::colIndex("Accessed") == 12);
    assert(DuckInode::colIndex("Modified") == 13);
    assert(DuckInode::colIndex("Metadata") == 14);
    auto idOffset = i + DuckInode::colIndex("Id");
    auto typeOffset = i + DuckInode::colIndex("Type");
    auto flagsOffset = i + DuckInode::colIndex("Flags");
    auto addrOffset = i + DuckInode::colIndex("Addr");
    auto fsOffsetOffset = i + DuckInode::colIndex("FsOffset");
    auto filesizeOffset = i + DuckInode::colIndex("Filesize");
    auto uidOffset = i + DuckInode::colIndex("Uid");
    auto gidOffset = i + DuckInode::colIndex("Gid");
    auto linkTargetOffset = i + DuckInode::colIndex("LinkTarget");
    auto numLinksOffset = i + DuckInode::colIndex("NumLinks");
    auto seqNumOffset = i + DuckInode::colIndex("SeqNum");
    auto createdOffset = i + DuckInode::colIndex("Created");
    auto accessedOffset = i + DuckInode::colIndex("Accessed");
    auto modifiedOffset = i + DuckInode::colIndex("Modified");
    auto metadataOffset = i + DuckInode::colIndex("Metadata");
*/
    //assert(metadataOffset - idOffset == DuckInode::NumCols - 1);

    append(appender,
            Buf.data() + OffsetVals[i],
            Buf.data() + OffsetVals[i + 1],
            Buf.data() + OffsetVals[i + 2],
            OffsetVals[i + 3],
            OffsetVals[i + 4],
            OffsetVals[i + 5],
            OffsetVals[i + 6],
            OffsetVals[i + 7],
            Buf.data() + OffsetVals[i + 8],
            OffsetVals[i + 9],
            OffsetVals[i + 10],
            Buf.data() + OffsetVals[i + 11],
            Buf.data() + OffsetVals[i + 12],
            Buf.data() + OffsetVals[i + 13],
            Buf.data() + OffsetVals[i + 14]
    );
    state = duckdb_appender_end_row(appender);
    THROW_IF(state == DuckDBError, "Failed call to end_row");
    ++numRows;
    i += DuckInode::NumCols;
  }
  DuckBatch::clear();
  return numRows;
}

