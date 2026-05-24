#pragma once

#include "fieldhash.h"
#include "jsoncons_wrapper.h"
#include "treehasher.h"

struct Dirent;
struct Inode;

class RecordHasher {
public:
  FieldHash hashRun(const jsoncons::json& r);
  FieldHash hashStream(const jsoncons::json& r);
  FieldHash hashAttr(const jsoncons::json& r);
  FieldHash hashInode(const jsoncons::json& r);
  FieldHash hashInode(const Inode& r);
  FieldHash hashDirent(const jsoncons::json& r);
  FieldHash hashDirent(const Dirent& r);

  FieldHash hashInodeIdentity(
    std::string_view evidenceFileName,
    uint64_t fsByteOffset,
    uint64_t addr,
    uint64_t seqNum);

private:
  TreeHasher Hasher;
};
