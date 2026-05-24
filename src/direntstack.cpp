#include "direntstack.h"

#include "fieldhash.h"
#include "hex.h"
#include "recordhasher.h"
#include "throw.h"

bool DirentStack::empty() const {
  return Stack.empty();
}

const Dirent& DirentStack::top() const {
  return Stack.top().Rec;
}

void DirentStack::setFsContext(std::string evidenceFileName, uint64_t fsByteOffset) {
  // Precondition: callers must drain residual dirents under the previous
  // FS context before flipping. Release-mode catch (debug-only assert
  // would let multi-FS FK corruption slip past NDEBUG builds).
  THROW_IF(!Stack.empty(),
           "DirentStack must be drained before changing FS context");
  CurEvidenceFileName = std::move(evidenceFileName);
  CurFsByteOffset = fsByteOffset;
}

Dirent DirentStack::pop() {
  // pop the record and trim back the path
  Element& e = Stack.top();
  Path.resize(e.LastPathSepIndex);
  Dirent rec{std::move(e.Rec)};
  Stack.pop();

  // hash the dirent record (existing behavior — drives dirent.Id)
  rec.Id = RecHasher.hashDirent(rec).to_string();

  // compute inode FK hashes via the shared identity helper, using
  // the per-FS context set by setFsContext.
  rec.MetaId = RecHasher.hashInodeIdentity(
    CurEvidenceFileName, CurFsByteOffset, rec.MetaAddr, rec.MetaSeq).to_string();

  rec.ParentId = RecHasher.hashInodeIdentity(
    CurEvidenceFileName, CurFsByteOffset, rec.ParentAddr, rec.ParentSeq).to_string();

  return rec;
}

std::optional<Dirent> DirentStack::push(Dirent&& rec) {
  if (rec.Name == "." || rec.Name == "..") {
    rec.Path = Path;
    return rec;
  }

  const size_t len = Path.length();
  if (len > 0) {
    Path.append("/");
  }
  Path.append(rec.Name);

  rec.Path = Path;

  Stack.push({std::move(rec), len});
  return std::nullopt;
}
