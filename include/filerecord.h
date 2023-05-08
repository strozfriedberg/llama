#pragma once

#include <memory>
#include <string>

#include <hasher/api.h>

#include "jsoncons_wrapper.h"

class BlockSequence;

// This is just a placeholder
struct FileRecord {
  SFHASH_HashValues Hashes;
  jsoncons::json Doc;
  std::shared_ptr<BlockSequence> Blocks;

  FileRecord() = delete;

  FileRecord(const FileRecord& r) = default;

  FileRecord(FileRecord&& r) = default;

  FileRecord(jsoncons::json&& doc);

  FileRecord(jsoncons::json&& doc, std::shared_ptr<BlockSequence>&& bseq);

  FileRecord& operator=(const FileRecord& r) = default;

  FileRecord& operator=(FileRecord&& r) = default;

  std::string str() const;
};
