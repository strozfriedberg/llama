#pragma once

#include <optional>
#include <stack>
#include <string>

#include "direntbatch.h"

class RecordHasher;

class DirentStack {
public:
  DirentStack(RecordHasher& rh): RecHasher(rh) {}

  bool empty() const;

  const Dirent& top() const;

  // Hashes the current dirent, adds that hash to the parent, then pops it.
  Dirent pop();

  // Makes this dirent current. Returns the dirent immediately for "." and
  // ".." entries without modifying the path or stack.
  std::optional<Dirent> push(Dirent&& dirent);

  // Called once per filesystem before any dirents from that filesystem
  // are pushed. Drives MetaId/ParentId hashing in pop().
  void setFsContext(std::string evidenceFileName, uint64_t fsByteOffset);

private:
  struct Element {
    Dirent Rec;
    size_t LastPathSepIndex;
  };

  std::stack<Element> Stack;
  std::string         Path;
  RecordHasher&       RecHasher;

  std::string CurEvidenceFileName;
  uint64_t    CurFsByteOffset = 0;
};
