#pragma once

#include "fieldhash.h"
#include "jsoncons_wrapper.h"
#include "treehasher.h"

struct Dirent;

class RecordHasher {
public:
  FieldHash hashRun(const jsoncons::json& r);

  FieldHash hashStream(const jsoncons::json& r);

  FieldHash hashAttr(const jsoncons::json& r);

  FieldHash hashInode(const jsoncons::json& r);

  FieldHash hashDirent(const jsoncons::json& r);

  FieldHash hashDirent(const Dirent& r);

private:
  TreeHasher Hasher;
};
