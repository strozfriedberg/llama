#include "filerecord.h"

FileRecord::FileRecord(jsoncons::json&& doc):
  Doc(std::move(doc)),
  Blocks()
{}

FileRecord::FileRecord(jsoncons::json&& doc, std::shared_ptr<BlockSequence>&& bseq):
  Doc(std::move(doc)),
  Blocks(std::move(bseq))
{}

std::string FileRecord::str() const {
  return Doc.as<std::string>();
}
