#pragma once

#include <string>
#include <sstream>

#include <hasher.h>

#include "jsoncons_wrapper.h"

struct TSK_FS_FILE;

// This is just a placeholder
struct FileRecord {
  std::string Path;

  uint64_t Size;

  std::string _data;

  SFHASH_HashValues Hashes;

  const char* fileBegin() const { return _data.c_str(); }

  const char* fileEnd() const { return _data.c_str() + _data.size(); }

  jsoncons::json Doc;

  FileRecord(const std::string& path = "", uint64_t size = 0): Path(path), Size(size) {}

  FileRecord(TSK_FS_FILE* file);

  void updateDoc() {
    Doc["Path"] = Path;
    Doc["Size"] = Size;
  }

  std::string str() const {
    return Doc.as<std::string>();
  }
};
