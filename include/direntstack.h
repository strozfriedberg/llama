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
  // ".." entries with Id/MetaId/ParentId already computed; does not modify
  // the internal path or stack for those entries.
  std::optional<Dirent> push(Dirent&& dirent);

  // Called once per filesystem before any dirents from that filesystem
  // are pushed. Drives MetaId/ParentId hashing in pop() and push().
  void setFsContext(std::string evidenceFileName, uint64_t fsByteOffset);

private:
  // Computes rec.Id (dirent identity) plus rec.MetaId / rec.ParentId
  // (inode-identity FKs) under the current FS context. rec.Path must be
  // set to the final emitted path before calling.
  void assignIdentityHashes(Dirent& rec);

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
