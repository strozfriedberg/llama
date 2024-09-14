#pragma once

#include <memory>
#include <vector>
#include <string>

struct FileRecord;
struct Dirent;
struct Inode;

class OutputHandler {
public:
  virtual ~OutputHandler() {}

  virtual void outputImage(const FileRecord& rec) = 0;

  virtual void outputDirent(const Dirent& rec) = 0;

  virtual void outputInode(const FileRecord& rec) = 0;
  virtual void outputInode(const Inode& rec) = 0;

  virtual void outputInodes(const std::shared_ptr<std::vector<FileRecord>>& batch) = 0;

  virtual void outputSearchHit(const std::string&) = 0;

  virtual void outputSearchHits(const std::vector<std::string>& batch) = 0;

  virtual void close() = 0;
};
