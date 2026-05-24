# Inode Id Primary Key Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Populate `inode.Id` as a cross-image-unique 64-char hex hash and reference it as a foreign key from `dirent`, `hash`, `extents`, and `filesystems`, so multi-image runs no longer collapse rows from different evidence files into a single `MetaAddr` namespace.

**Architecture:** Add a single shared four-input identity hash (`hashInodeIdentity` over `EvidenceFileName`, `FsByteOffset`, `Addr`, `SeqNum`) on `RecordHasher`. Compute it canonically on the `Inode` side (`TskReader::addToBatch`), plumb the string through `Entry::InodeId` to the `Processor`/`HashRec` writer, and reconstruct it from per-FS context in `DirentStack::pop` for `dirent.MetaId`/`ParentId`. Schema columns added on `dirent` (`MetaId`, `ParentId`), `hash` (`InodeId` replaces `MetaAddr`), `extents` (`InodeId` replaces `Inode` + `FilesystemOffset`), `filesystems` (`RootInodeId`).

**Tech Stack:** C++20, Catch2 v3 (tests), meson+ninja (build), DuckDB (output), TSK (filesystem walk).

**Spec:** [`docs/superpowers/specs/2026-05-23-inode-id-primary-key-design.md`](../specs/2026-05-23-inode-id-primary-key-design.md)

---

## File Structure

**Headers modified:**
- `include/recordhasher.h` — add `hashInodeIdentity` and `hashInode(const Inode&)` declarations
- `include/direntbatch.h` — widen `Dirent` struct (in-struct EvidenceFileName/FsByteOffset; new ColNames-emitted MetaId/ParentId)
- `include/duckhash.h` — swap `HashRec::MetaAddr` for `InodeId`; new `set` signature
- `include/entry.h` — add `Entry::InodeId`
- `include/evidencerec.h` — add `FilesystemRec::RootInodeId`
- `include/tskimgassembler.h` — add `setCurrentRootInodeId` member
- `include/extent.h` — drop `Inode` + `FilesystemOffset`, add `InodeId`

**Sources modified:**
- `src/recordhasher.cpp` — implement the new methods
- `src/tskreader.cpp` — set `inode.Id`, plumb `entry->InodeId`, set dirent context, call `setCurrentRootInodeId`
- `src/direntstack.cpp` — compute `MetaId` and `ParentId` in `pop()`
- `src/processor.cpp` — pass `entry.InodeId` to `HashRec::set`
- `src/tskimgassembler.cpp` — `RootInodeId = ""` initial; implement `setCurrentRootInodeId`
- `src/posixreader.cpp` — populate `Extent::InodeId = ""` at construction sites; add TODO comment by the existing `inode.Id = "";`

**Tests modified:**
- `test/test_recordhasher.cpp` — golden-vector tests for new helper + overload
- `test/test_direntstack.cpp` — assert `MetaId`/`ParentId` populated after pop
- `test/test_tskimgassembler.cpp` — assert `RootInodeId` round-trips
- `test/test_processor.cpp` — update HashRec positional inits and `SELECT MetaAddr` query

---

## Working with the build

The test binary is `build/test/llama_test`. Re-run via:
```bash
cd /Users/jon/code/llama/build && ninja llama_test && ./test/llama_test
```
Run a single Catch2 test case:
```bash
./build/test/llama_test "testHashInodeIdentity"
```

After every task's tests pass, run the full suite (`./build/test/llama_test`) before committing.

---

## Task 1: Shared `hashInodeIdentity` helper + `hashInode(const Inode&)` overload

**Files:**
- Modify: `include/recordhasher.h`
- Modify: `src/recordhasher.cpp`
- Test: `test/test_recordhasher.cpp`

The existing `RecordHasher::hashInode(const jsoncons::json&)` is unused dead code from the old fsrip pipeline — leave it alone. The new overload `hashInode(const Inode&)` is what production calls, and it delegates to a shared `hashInodeIdentity` helper that the dirent path will also use.

- [ ] **Step 1: Write the failing test for the helper (golden vector)**

Append to `test/test_recordhasher.cpp`:

```cpp
TEST_CASE("testHashInodeIdentity") {
  RecordHasher hasher;
  // Inputs: ("disk.E01", 1048576, 5, 5)
  const FieldHash got = hasher.hashInodeIdentity("disk.E01", 1048576, 5, 5);
  // Round-trip: hashing the same inputs again must produce the same bytes.
  const FieldHash again = hasher.hashInodeIdentity("disk.E01", 1048576, 5, 5);
  REQUIRE(got == again);
  // And a different SeqNum must produce a different hash (proves SeqNum is hashed).
  const FieldHash diffSeq = hasher.hashInodeIdentity("disk.E01", 1048576, 5, 6);
  REQUIRE(got != diffSeq);
  // Different EvidenceFileName must also differ.
  const FieldHash diffEfn = hasher.hashInodeIdentity("other.E01", 1048576, 5, 5);
  REQUIRE(got != diffEfn);
}
```

- [ ] **Step 2: Run test to verify it fails (method does not exist)**

```bash
cd /Users/jon/code/llama/build && ninja llama_test 2>&1 | tail -5
```
Expected: compile error — `'hashInodeIdentity' is not a member of 'RecordHasher'`.

- [ ] **Step 3: Add declaration to `include/recordhasher.h`**

Add the forward declaration for `Inode` and the new method declarations:

```cpp
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
```

- [ ] **Step 4: Implement the helper and overload in `src/recordhasher.cpp`**

Add `#include "inode.h"` near the top (next to `#include "direntbatch.h"`).
Append to the end of the file:

```cpp
FieldHash RecordHasher::hashInodeIdentity(
  std::string_view evidenceFileName,
  uint64_t fsByteOffset,
  uint64_t addr,
  uint64_t seqNum)
{
  auto h = Hasher.subhash();
  Hasher.hash_em(evidenceFileName, fsByteOffset, addr, seqNum);
  return Hasher.get_hash();
}

FieldHash RecordHasher::hashInode(const Inode& r) {
  return hashInodeIdentity(r.EvidenceFileName, r.ByteOffset, r.Addr, r.SeqNum);
}
```

- [ ] **Step 5: Add a golden-vector test for `hashInode(const Inode&)` matching `hashInodeIdentity`**

Append to `test/test_recordhasher.cpp` (after `testHashInodeIdentity`):

```cpp
TEST_CASE("testHashInodeStruct") {
  RecordHasher hasher;
  Inode inode{};
  inode.EvidenceFileName = "disk.E01";
  inode.ByteOffset = 1048576;
  inode.Addr = 5;
  inode.SeqNum = 5;
  const FieldHash viaStruct = hasher.hashInode(inode);
  const FieldHash viaHelper = hasher.hashInodeIdentity("disk.E01", 1048576, 5, 5);
  REQUIRE(viaStruct == viaHelper);
}
```

`Inode` is already included transitively via `recordhasher.cpp`'s `#include "inode.h"`. The test file already includes `recordhasher.h`; add `#include "inode.h"` near the top of `test_recordhasher.cpp` too.

- [ ] **Step 6: Build and run the new tests**

```bash
cd /Users/jon/code/llama/build && ninja llama_test && ./test/llama_test "testHashInodeIdentity" "testHashInodeStruct"
```
Expected: both PASS.

- [ ] **Step 7: Run the full suite to confirm no regressions**

```bash
./test/llama_test
```
Expected: all PASS.

- [ ] **Step 8: Commit**

```bash
git add include/recordhasher.h src/recordhasher.cpp test/test_recordhasher.cpp
git commit -m "feat(recordhasher): add hashInodeIdentity + hashInode(Inode) overload

Shared identity hash over (EvidenceFileName, FsByteOffset, Addr, SeqNum)
that will back inode.Id and the dirent FK hashes."
```

---

## Task 2: Populate `inode.Id` and `Entry::InodeId` in `TskReader::addToBatch`

**Files:**
- Modify: `include/entry.h`
- Modify: `src/tskreader.cpp:137-152`

`Entry` is created next to the `Inode` in the same per-FS scope. We compute the Id once on the Inode side and copy it onto the Entry so `Processor::process` doesn't need to re-hash.

- [ ] **Step 1: Add `InodeId` field to `Entry`**

Edit `include/entry.h` — add after the `FsOffset` line:

```cpp
class Entry {
public:
  Entry(uint64_t addr) : Addr(addr) {}
  Entry(uint64_t addr, std::unique_ptr<ReadSeek> rs) : Addr(addr), stream(std::move(rs)) {}
  ReadSeek& getStream() { return *stream; }
  void setStream(std::unique_ptr<ReadSeek> rs) { stream = std::move(rs); }
  bool hasStream() const { return stream != nullptr; }

  uint64_t Addr;
  std::string EvidenceFile;
  uint32_t    FsIndex = 0;
  uint64_t    FsOffset = 0;
  std::string InodeId;
  uint32_t    AddrFlags = 0;
  std::string Path;
  uint64_t    FileSize = 0;
  uint64_t    DiskOffset = 0;
  TSK_FS_TYPE_ENUM FsType = TSK_FS_TYPE_DETECT;

private:
  std::unique_ptr<ReadSeek> stream;
};
```

- [ ] **Step 2: Compute `inode.Id` and assign to `entry->InodeId` in `TskReader::addToBatch`**

Edit `src/tskreader.cpp`. The block at lines ~136–156 currently sets `inode.EvidenceFileName`, `inode.ByteOffset`, then pushes the inode and constructs the Entry. Insert the hash computation just before `Input->push(inode);` and the assignment just after the entry is constructed.

Add `#include "hex.h"` and `#include "recordhasher.h"` to the top of `src/tskreader.cpp` if not already present. (`recordhasher.h` is already pulled in transitively via `direntstack.h` → `recordhasher.h`, but include it explicitly for clarity.)

The relevant block becomes:

```cpp
  if (!InodeTracker.at(meta.addr - fs_file->fs_info->first_inum)) {
    Inode inode;
    TskUtils::convertMetaToInode(meta, *Tsg, inode);
    inode.EvidenceFileName = std::filesystem::path(ImgPath).filename().string();
    inode.ByteOffset = CurFsOffset;
    inode.Id = hexEncode(RecHasher.hashInode(inode).hash, sizeof(FieldHash{}.hash));

    // handle the attrs
    Tsk->populateAttrs(fs_file);

    Input->push(inode);
    auto entry = std::make_unique<Entry>(meta.addr);
    entry->EvidenceFile = ImgPath;
    entry->FsIndex = Asm.fsIndex();
    entry->FsOffset = CurFsOffset;
    entry->FsType = fs_file->fs_info->ftype;
    entry->AddrFlags = meta.flags;
    entry->Path = path ? path : "";
    entry->FileSize = meta.size;
    entry->InodeId = inode.Id;
```

(Note: `sizeof(FieldHash{}.hash)` evaluates to 32 at compile time; you can also write the literal `32`.)

- [ ] **Step 3: Build everything that depends on Entry and TskReader**

```bash
cd /Users/jon/code/llama/build && ninja llama_test 2>&1 | tail -10
```
Expected: clean build.

- [ ] **Step 4: Run the full test suite**

```bash
./test/llama_test
```
Expected: all PASS. No existing test asserts that `Entry::InodeId` or `Inode::Id` are populated — this change is non-breaking until consumers start using the values.

- [ ] **Step 5: Commit**

```bash
git add include/entry.h src/tskreader.cpp
git commit -m "feat(tskreader): populate inode.Id and Entry::InodeId

Canonical inode identity hash computed once per inode in addToBatch
and plumbed downstream via Entry."
```

---

## Task 3: Swap `HashRec::MetaAddr` for `InodeId`

**Files:**
- Modify: `include/duckhash.h`
- Modify: `src/processor.cpp:143`
- Modify: `test/test_processor.cpp:240-241, 245, 333, 339-341`

`HashRec`'s primary identifier becomes the inode Id string. The `set()` signature changes accordingly.

- [ ] **Step 1: Update `HashRec` in `include/duckhash.h`**

Replace the struct body:

```cpp
#pragma once

#include <hasher/api.h>

#include "hex.h"
#include "llamaduck.h"

struct HashRec {
  void set(SFHASH_HashValues h, std::string inodeId, uint64_t hashAlgs) {
    InodeId = std::move(inodeId);
    MD5 = hashAlgs & SFHASH_MD5 ? hexEncode(h.Md5, h.Md5 + sizeof(h.Md5)) : "";
    SHA1 = hashAlgs & SFHASH_SHA_1 ? hexEncode(h.Sha1, h.Sha1 + sizeof(h.Sha1)): "";
    SHA256 = hashAlgs & SFHASH_SHA_2_256 ? hexEncode(h.Sha2_256, h.Sha2_256 + sizeof(h.Sha2_256)): "";
    Blake3 = hashAlgs & SFHASH_BLAKE3 ? hexEncode(h.Blake3, h.Blake3 + sizeof(h.Blake3)): "";
    Ssdeep = hashAlgs & SFHASH_FUZZY ? hexEncode(h.Fuzzy, h.Fuzzy + sizeof(h.Fuzzy)): "";
  }

  static constexpr auto ColNames = {"InodeId",
                                    "MD5",
                                    "SHA1",
                                    "SHA256",
                                    "Blake3",
                                    "Ssdeep"};

  std::string InodeId;

  std::string MD5;
  std::string SHA1;
  std::string SHA256;
  std::string Blake3;
  std::string Ssdeep;
};

using HashBatch = DBBatch<HashRec>;
```

- [ ] **Step 2: Update the call site in `src/processor.cpp:143`**

Change:
```cpp
  HashRecord.set(h, entry.Addr, HashAlgs);
```
to:
```cpp
  HashRecord.set(h, entry.InodeId, HashAlgs);
```

- [ ] **Step 3: Update positional HashRec inits in `test/test_processor.cpp:240-245`**

Change:
```cpp
  proc.hashBatch()->add(HashRec{1, "md5_1", "sha1_1", "sha256_1", "blake3_1", "ssdeep_1"});
  proc.hashBatch()->add(HashRec{2, "md5_2", "sha1_2", "sha256_2", "blake3_2", "ssdeep_2"});
  ...
  proc.hashBatch()->add(HashRec{3, "md5_3", "sha1_3", "sha256_3", "blake3_3", "ssdeep_3"});
```
to:
```cpp
  proc.hashBatch()->add(HashRec{"id_1", "md5_1", "sha1_1", "sha256_1", "blake3_1", "ssdeep_1"});
  proc.hashBatch()->add(HashRec{"id_2", "md5_2", "sha1_2", "sha256_2", "blake3_2", "ssdeep_2"});
  ...
  proc.hashBatch()->add(HashRec{"id_3", "md5_3", "sha1_3", "sha256_3", "blake3_3", "ssdeep_3"});
```

- [ ] **Step 4: Fix the `processBatch sorts entries by DiskOffset` test query**

In `test/test_processor.cpp` (around lines 313–342), entries are created with `Addr=100/200/300` but no `InodeId`. To preserve the test intent (verify processing order), set `InodeId` on each entry to a known value and query that column instead of `MetaAddr`.

Replace the entry-creation block and the final assertions:

```cpp
  auto e1 = std::make_unique<Entry>(100, std::make_unique<ReadSeekBuf>("aaa"));
  e1->DiskOffset = 3000;
  e1->InodeId = "id_100";
  entries->push_back(std::move(e1));

  auto e2 = std::make_unique<Entry>(200, std::make_unique<ReadSeekBuf>("bbb"));
  e2->DiskOffset = 1000;
  e2->InodeId = "id_200";
  entries->push_back(std::move(e2));

  auto e3 = std::make_unique<Entry>(300, std::make_unique<ReadSeekBuf>("ccc"));
  e3->DiskOffset = 2000;
  e3->InodeId = "id_300";
  entries->push_back(std::move(e3));

  proc.processBatch(entries);

  // Query the hash table; rows are inserted in processing order
  duckdb_result result;
  duckdb_query(conn.get(), "SELECT InodeId FROM hash", &result);
  auto rowCount = duckdb_row_count(&result);
  REQUIRE(rowCount == 3);

  // If sorted by DiskOffset, processing order should be: 1000, 2000, 3000
  // which corresponds to InodeId values: id_200, id_300, id_100
  REQUIRE(std::string(duckdb_value_varchar(&result, 0, 0)) == "id_200");
  REQUIRE(std::string(duckdb_value_varchar(&result, 0, 1)) == "id_300");
  REQUIRE(std::string(duckdb_value_varchar(&result, 0, 2)) == "id_100");
  duckdb_destroy_result(&result);
```

(Add `#include <string>` to `test/test_processor.cpp` if not already present.)

- [ ] **Step 5: Build and run the processor test**

```bash
cd /Users/jon/code/llama/build && ninja llama_test && ./test/llama_test "[processor]"
```
If Catch2 tags aren't used, run `./test/llama_test "processBatch sorts entries by DiskOffset for sequential I/O"` instead.
Expected: PASS.

- [ ] **Step 6: Run the full suite**

```bash
./test/llama_test
```
Expected: all PASS.

- [ ] **Step 7: Commit**

```bash
git add include/duckhash.h src/processor.cpp test/test_processor.cpp
git commit -m "feat(hash): replace HashRec.MetaAddr with InodeId

HashRec now keys on inode.Id (string). Processor passes
entry.InodeId precomputed by TskReader."
```

---

## Task 4: Add `MetaId`/`ParentId` to `Dirent`; carry per-FS context on `DirentStack`

**Files:**
- Modify: `include/direntbatch.h`
- Modify: `include/direntstack.h`
- Modify: `src/direntstack.cpp:13-33`
- Modify: `src/tskreader.cpp` (filterFs and addToBatch)
- Modify: `test/test_direntstack.cpp:6-25, 33-46, 48-63`
- Modify: `test/test_recordhasher.cpp:226-254` (existing `testHashDirentClass`)

**Why DirentStack carries the context, not Dirent itself:** `include/llamaduck.h:163` enforces `static_assert(ColNames.size() == NumCols)` via `boost::pfr::structure_tie`. Adding fields to `Dirent` that aren't in `ColNames` won't compile. So the per-FS context (`EvidenceFileName`, `FsByteOffset`) lives on `DirentStack` and is set once per filesystem; `pop()` uses those members when computing `MetaId`/`ParentId`. The dirent struct gains only the two emitted columns (`MetaId`, `ParentId`), keeping the field-count-to-ColNames invariant satisfied. (This is a faithful realization of the spec's intent — the context is not emitted to parquet — via a mechanism the codebase supports.)

- [ ] **Step 1: Widen the `Dirent` struct with only the two emitted columns**

Replace `include/direntbatch.h`:

```cpp
#pragma once

#include <string>

#include <duckdb.h>

#include "llamaduck.h"

struct Dirent
{
  static constexpr auto ColNames = {"Id",
                                    "Path",
                                    "Name",
                                    "ShortName",
                                    "Type",
                                    "Flags",
                                    "MetaAddr",
                                    "ParentAddr",
                                    "MetaSeq",
                                    "ParentSeq",
                                    "MetaId",
                                    "ParentId"};

  std::string Id;
  std::string Path;
  std::string Name;
  std::string ShortName;

  std::string Type;
  std::string Flags;

  uint64_t MetaAddr;
  uint64_t ParentAddr;
  uint64_t MetaSeq;
  uint64_t ParentSeq;

  std::string MetaId;
  std::string ParentId;
};

using DirentBatch = DBBatch<Dirent>;
```

Field order matches `ColNames` order one-to-one (12 fields, 12 names). `boost::pfr::structure_tie` iterates in declaration order, and the `DBBatch::add` path maps each tuple element to its corresponding `ColNames` entry positionally.

- [ ] **Step 2: Add per-FS context members and setter to `DirentStack`**

Replace `include/direntstack.h`:

```cpp
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
```

- [ ] **Step 3: Implement `setFsContext` and rewrite `pop()` to compute the FK hashes**

Replace `src/direntstack.cpp`:

```cpp
#include "direntstack.h"
#include "fieldhash.h"
#include "hex.h"
#include "recordhasher.h"

bool DirentStack::empty() const {
  return Stack.empty();
}

const Dirent& DirentStack::top() const {
  return Stack.top().Rec;
}

void DirentStack::setFsContext(std::string evidenceFileName, uint64_t fsByteOffset) {
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
  const FieldHash fhash{RecHasher.hashDirent(rec)};
  rec.Id = hexEncode(&fhash.hash, sizeof(fhash.hash));

  // compute inode FK hashes via the shared identity helper, using
  // the per-FS context set by setFsContext.
  const FieldHash metaHash = RecHasher.hashInodeIdentity(
    CurEvidenceFileName, CurFsByteOffset, rec.MetaAddr, rec.MetaSeq);
  rec.MetaId = hexEncode(&metaHash.hash, sizeof(metaHash.hash));

  const FieldHash parentHash = RecHasher.hashInodeIdentity(
    CurEvidenceFileName, CurFsByteOffset, rec.ParentAddr, rec.ParentSeq);
  rec.ParentId = hexEncode(&parentHash.hash, sizeof(parentHash.hash));

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
```

- [ ] **Step 4: Wire `setFsContext` from `TskReader::filterFs`**

Edit `src/tskreader.cpp`. In `filterFs` (around line 87–119), after `CurFsOffset = fs_info->offset;`, add:

```cpp
  Dirents.setFsContext(
    std::filesystem::path(ImgPath).filename().string(),
    CurFsOffset);
```

(No change to `addToBatch` is needed for the dirent path — the context is now stack-wide.)

- [ ] **Step 5: Update `test_direntstack.cpp` operators and `makeDirent` helper**

In `test/test_direntstack.cpp`, the `operator<<`, `operator==`, and `makeDirent` need to know about the new MetaId/ParentId fields. Replace lines 6–46:

```cpp
std::ostream& operator<<(std::ostream& os, const Dirent& dirent) {
  os << "{\"Id\":\"" << dirent.Id << "\", \"Path\": \"" << dirent.Path << "\", \"Name\": \"" << dirent.Name <<
    "\", \"ShortName\": \"" << dirent.ShortName << "\", \"Type\": \"" << dirent.Type << "\", \"Flags\": \"" << dirent.Flags <<
    "\", \"MetaAddr\": " << dirent.MetaAddr << ", \"ParentAddr\": " << dirent.ParentAddr << ", \"MetaSeq\": " << dirent.MetaSeq <<
    ", \"ParentSeq\": " << dirent.ParentSeq <<
    ", \"MetaId\": \"" << dirent.MetaId << "\", \"ParentId\": \"" << dirent.ParentId << "\"}";
  return os;
}

bool operator==(const Dirent& l, const Dirent& r) {
  return l.Id == r.Id &&
         l.Path == r.Path &&
         l.Name == r.Name &&
         l.ShortName == r.ShortName &&
         l.Type == r.Type &&
         l.Flags == r.Flags &&
         l.MetaAddr == r.MetaAddr &&
         l.ParentAddr == r.ParentAddr &&
         l.MetaSeq == r.MetaSeq &&
         l.ParentSeq == r.ParentSeq &&
         l.MetaId == r.MetaId &&
         l.ParentId == r.ParentId;
}

TEST_CASE("testDirentStackStartsEmpty") {
  RecordHasher rh;
  DirentStack dirents(rh);
  REQUIRE(dirents.empty());
}

Dirent makeDirent(const std::string& path, const std::string& name) {
  Dirent d{};
  d.Path = path;
  d.Name = name;
  return d;
}
```

(Using value-initialized struct + field assignments is more robust than positional brace init.)

- [ ] **Step 6: Update `testDirentStackPushPop` to set FS context and assert MetaId/ParentId**

Replace `testDirentStackPushPop` (around lines 48–63):

```cpp
TEST_CASE("testDirentStackPushPop") {
  RecordHasher rh;
  DirentStack dirents(rh);
  dirents.setFsContext("disk.E01", 1048576);

  Dirent in(makeDirent("", "the name"));
  in.MetaAddr = 100;
  in.MetaSeq = 1;
  in.ParentAddr = 5;
  in.ParentSeq = 5;

  dirents.push(std::move(in));

  REQUIRE(!dirents.empty());
  REQUIRE("the name" == dirents.top().Path);

  Dirent out = dirents.pop();
  REQUIRE(out.Path == "the name");
  REQUIRE(out.Name == "the name");
  REQUIRE_FALSE(out.Id.empty());
  REQUIRE_FALSE(out.MetaId.empty());
  REQUIRE_FALSE(out.ParentId.empty());

  // MetaId must equal hashInodeIdentity over the same inputs.
  const FieldHash expMeta = rh.hashInodeIdentity("disk.E01", 1048576, 100, 1);
  REQUIRE(out.MetaId == hexEncode(&expMeta.hash, sizeof(expMeta.hash)));

  // ParentId likewise.
  const FieldHash expParent = rh.hashInodeIdentity("disk.E01", 1048576, 5, 5);
  REQUIRE(out.ParentId == hexEncode(&expParent.hash, sizeof(expParent.hash)));
}
```

Add `#include "hex.h"` and `#include "fieldhash.h"` to the top of `test_direntstack.cpp` if not already present.

If other test cases in the file (e.g., `testDirentStackPushPushPopPop`) also call `pop()`, they don't strictly need to assert MetaId/ParentId but they DO need `setFsContext` called once before any pops — otherwise MetaId/ParentId will be empty-string-context hashes (still deterministic, but meaningless). Add `dirents.setFsContext("", 0);` to those tests, or update them to set realistic context. Tests that only call `push`/`top`/`empty` without `pop` need no change.

- [ ] **Step 7: Fix `testHashDirentClass` in `test_recordhasher.cpp`**

The existing positional brace init at lines 227–250 uses 10 values. With the new fields added at the end, missing-value defaulting will zero-init them — but only if `ColNames` order matches struct order and the struct doesn't reorder. Safer: rewrite as value-init + assignments:

```cpp
TEST_CASE("testHashDirentClass") {
  Dirent d1{};
  d1.Path = "/foo/bar";
  d1.Name = "baz";
  d1.ShortName = "b";
  d1.Type = "File";
  d1.MetaAddr = 0x12345678;
  d1.ParentAddr = 0x87654321;
  d1.MetaSeq = 0x87654321;
  d1.ParentSeq = 0x12345678;

  Dirent d2 = d1;
  d2.Flags = "Deleted";

  RecordHasher hasher;
  REQUIRE(hasher.hashDirent(d1) != hasher.hashDirent(d2));
}
```

- [ ] **Step 8: Build and run the affected tests**

```bash
cd /Users/jon/code/llama/build && ninja llama_test && ./test/llama_test "testDirentStackPushPop" "testHashDirentClass"
```
Expected: PASS.

- [ ] **Step 9: Run any other direntstack tests**

```bash
./test/llama_test "testDirentStack*"
```
Expected: all PASS. If others (like `testDirentStackPushPushPopPop`) need updating because they used positional inits, apply the same value-init pattern.

- [ ] **Step 10: Run the full suite**

```bash
./test/llama_test
```
Expected: all PASS.

- [ ] **Step 11: Commit**

```bash
git add include/direntbatch.h include/direntstack.h src/direntstack.cpp src/tskreader.cpp test/test_direntstack.cpp test/test_recordhasher.cpp
git commit -m "feat(dirent): add MetaId and ParentId FK columns

Dirent gains two emitted columns (MetaId, ParentId) populated by
DirentStack::pop via the shared hashInodeIdentity helper. Per-FS
context (EvidenceFileName, FsByteOffset) lives on DirentStack and is
set once per filesystem from TskReader::filterFs — keeping the Dirent
field-count-to-ColNames invariant intact while still excluding the
context from parquet output. MetaId/ParentId match the inode.Id
byte-for-byte when MetaSeq/ParentSeq match the live inode's SeqNum."
```

---

## Task 5: Add `RootInodeId` to `filesystems` and populate via lazy setter

**Files:**
- Modify: `include/evidencerec.h:56-98`
- Modify: `include/tskimgassembler.h:9-60`
- Modify: `src/tskimgassembler.cpp:62-108`
- Modify: `src/tskreader.cpp:137-156`
- Modify: `test/test_tskimgassembler.cpp` (if existing tests assert FilesystemRec column count)

`addFileSystem` is called before the inode walk starts, so we can't compute `RootInodeId` there. Instead, the assembler exposes a setter; `TskReader::addToBatch` calls it when it sees the root inode (`meta.addr == fs_file->fs_info->root_inum`).

- [ ] **Step 1: Add `RootInodeId` to `FilesystemRec`**

In `include/evidencerec.h`, add `"RootInodeId"` to the `ColNames` list and add the field:

```cpp
struct FilesystemRec {
  static constexpr auto ColNames = {"EvidenceFileName",
                                    "ByteOffset",
                                    "VolumeAddr",
                                    "VolumeTableNum",
                                    "Type",
                                    "BlockSize",
                                    "BlockCount",
                                    "DeviceBlockSize",
                                    "BlockName",
                                    "LittleEndian",
                                    "FirstBlock",
                                    "FirstInum",
                                    "LastBlock",
                                    "LastInum",
                                    "Flags",
                                    "FsID",
                                    "JournalInum",
                                    "RootInum",
                                    "NumInums",
                                    "RootDirentId",
                                    "RootInodeId"};

  std::string EvidenceFileName;
  uint64_t    ByteOffset;
  uint64_t    VolumeAddr;
  uint64_t    VolumeTableNum;
  std::string Type;
  uint64_t    BlockSize;
  uint64_t    BlockCount;
  uint64_t    DeviceBlockSize;
  std::string BlockName;
  uint64_t    LittleEndian;
  uint64_t    FirstBlock;
  uint64_t    FirstInum;
  uint64_t    LastBlock;
  uint64_t    LastInum;
  std::string Flags;
  std::string FsID;
  uint64_t    JournalInum;
  uint64_t    RootInum;
  uint64_t    NumInums;
  std::string RootDirentId;
  std::string RootInodeId;
};
```

- [ ] **Step 2: Add `setCurrentRootInodeId` to `TskImgAssembler`**

In `include/tskimgassembler.h`, add the public method declaration:

```cpp
  void setCurrentRootInodeId(const std::string& id);
```

- [ ] **Step 3: Initialize `RootInodeId` in `addFileSystem` and implement the setter**

In `src/tskimgassembler.cpp`, in `addFileSystem` (around line 89), add after `fs.RootDirentId = "";`:

```cpp
  fs.RootInodeId = "";
```

Append the setter implementation to the file:

```cpp
void TskImgAssembler::setCurrentRootInodeId(const std::string& id) {
  if (!Filesystems.empty() && Filesystems.back().RootInodeId.empty()) {
    Filesystems.back().RootInodeId = id;
  }
}
```

- [ ] **Step 4: Call the setter from `TskReader::addToBatch` when the root inode is emitted**

In `src/tskreader.cpp`, right after `inode.Id = hexEncode(...)` (and before `Input->push(inode);`), add:

```cpp
    if (meta.addr == fs_file->fs_info->root_inum) {
      Asm.setCurrentRootInodeId(inode.Id);
    }
```

- [ ] **Step 5: Write a test asserting the setter populates the last filesystem**

In `test/test_tskimgassembler.cpp`, append:

```cpp
TEST_CASE("testSetCurrentRootInodeId") {
  TskImgAssembler asm_;
  asm_.addImage("disk.E01", "/path/disk.E01", "ewf", "Expert Witness", 1024, 512, "");
  asm_.addFileSystem(1048576, "ntfs", 4096, 100, 512, "byte",
                     true, 0, 0, 99, 99, "", "abcd1234",
                     0, 5, 100);

  REQUIRE(asm_.filesystems().size() == 1);
  REQUIRE(asm_.filesystems().back().RootInodeId == "");

  asm_.setCurrentRootInodeId("deadbeef");
  REQUIRE(asm_.filesystems().back().RootInodeId == "deadbeef");

  // Second call must NOT overwrite the first (idempotent on the same FS).
  asm_.setCurrentRootInodeId("cafebabe");
  REQUIRE(asm_.filesystems().back().RootInodeId == "deadbeef");
}
```

(The assembler may require a particular state-machine progression for `addFileSystem`; if `addImage` alone isn't sufficient state, mirror the setup from existing tests in the same file.)

- [ ] **Step 6: Build and run the new test**

```bash
cd /Users/jon/code/llama/build && ninja llama_test && ./test/llama_test "testSetCurrentRootInodeId"
```
Expected: PASS.

- [ ] **Step 7: Run the full suite**

```bash
./test/llama_test
```
Expected: all PASS.

- [ ] **Step 8: Commit**

```bash
git add include/evidencerec.h include/tskimgassembler.h src/tskimgassembler.cpp src/tskreader.cpp test/test_tskimgassembler.cpp
git commit -m "feat(filesystems): add RootInodeId column

Populated lazily by TskReader when the root inode is emitted; the
assembler exposes a setCurrentRootInodeId that writes to the most
recently added FilesystemRec. RootDirentId field is retained
(still empty) for backwards compatibility."
```

---

## Task 6: Replace `Extent.Inode`/`FilesystemOffset` with `InodeId`

**Files:**
- Modify: `include/extent.h`
- Modify: `src/posixreader.cpp:162-200` (the `parseExtents` extent construction)

`Extent` rows today are only produced by `PosixReader` (the TSK path doesn't emit extents). Since `PosixReader` leaves `inode.Id` empty by design, extents from `PosixReader` will have `InodeId = ""` — consistent with the design's "PosixReader is broken / out of scope" decision.

- [ ] **Step 1: Update the `Extent` struct**

Replace `include/extent.h`:

```cpp
// ABOUTME: Extent struct representing a physical disk extent
// ABOUTME: Maps logical file offsets to physical disk locations via FIEMAP

#pragma once

#include <cstdint>
#include <string>

struct Extent {
    static constexpr auto ColNames = {
        "PhysicalStart",
        "PhysicalEnd",
        "LogicalStart",
        "LogicalEnd",
        "InodeId",
        "Path",
        "Flags",
        "Source"
    };

    uint64_t PhysicalStart;
    uint64_t PhysicalEnd;
    uint64_t LogicalStart;
    uint64_t LogicalEnd;
    std::string InodeId;
    std::string Path;
    std::string Flags;   // Comma-separated: "SHARED,UNWRITTEN,ENCODED"
    std::string Source;  // "filesystem" or "journal"
};
```

- [ ] **Step 2: Update construction in `src/posixreader.cpp`**

Find every site in `src/posixreader.cpp` that sets `ext.Inode = ...;` and `ext.FilesystemOffset = ...;`. Read the file around line 162 first to see all assignment sites in `parseExtents`. Replace each `ext.Inode = X;` / `ext.FilesystemOffset = Y;` pair with `ext.InodeId = "";` (PosixReader cannot compute a real InodeId because it leaves `inode.Id` empty per the design).

For example, around line 162:

```cpp
Extent ext;
ext.PhysicalStart = ...;
ext.PhysicalEnd = ...;
ext.LogicalStart = ...;
ext.LogicalEnd = ...;
ext.InodeId = "";   // was: ext.Inode = inum; ext.FilesystemOffset = fsOffset;
ext.Path = ...;
ext.Flags = ...;
ext.Source = ...;
```

(If `parseExtents` takes the inode number / fs offset as parameters, leave the parameters in place but stop using them on the `Extent`. Removing the parameters can be a followup; for now, suppress any unused-parameter warnings with `(void)inum;` etc. if they appear.)

- [ ] **Step 3: Build and confirm clean compile**

```bash
cd /Users/jon/code/llama/build && ninja llama_test 2>&1 | tail -10
```
Expected: clean build (no extent tests exist to update; if any do, fix them with the same field rename).

- [ ] **Step 4: Run the full suite**

```bash
./test/llama_test
```
Expected: all PASS.

- [ ] **Step 5: Commit**

```bash
git add include/extent.h src/posixreader.cpp
git commit -m "feat(extents): replace Inode + FilesystemOffset with InodeId

PosixReader is the only producer of Extent rows today; its
inode.Id is left empty by design, so extent.InodeId is empty too.
TSK path doesn't emit extents."
```

---

## Task 7: Add PosixReader TODO comment referencing the design

**Files:**
- Modify: `src/posixreader.cpp:131`

- [ ] **Step 1: Replace the existing comment**

Change:
```cpp
    inode.Id = "";  // Caller can generate hash
```
to:
```cpp
    // TODO: PosixReader is suspected broken and will be revisited.
    // When it is, populate inode.Id via RecordHasher::hashInode(inode) using
    // a meaningful (EvidenceFileName, FsByteOffset, Addr, SeqNum) tuple. See
    // docs/superpowers/specs/2026-05-23-inode-id-primary-key-design.md.
    inode.Id = "";
```

- [ ] **Step 2: Build to confirm no syntax issue**

```bash
cd /Users/jon/code/llama/build && ninja llama_test 2>&1 | tail -5
```
Expected: clean build.

- [ ] **Step 3: Commit**

```bash
git add src/posixreader.cpp
git commit -m "docs(posixreader): TODO referencing inode.Id design"
```

---

## Self-review checklist

Before declaring the plan complete, the implementer should manually verify:

- [ ] `inode.Id` is a 64-char lowercase hex string for every row produced by a TSK run.
- [ ] `dirent.MetaId` and `dirent.ParentId` are non-empty for every row whose `MetaAddr`/`ParentAddr` is non-zero.
- [ ] `hash.InodeId` is non-empty and matches some `inode.Id`.
- [ ] `filesystems.RootInodeId` is non-empty for every filesystem.
- [ ] A LEFT JOIN `dirent.MetaId = inode.Id` resolves for non-orphan dirents and yields NULL on the inode side for orphan / stale-seq dirents.
- [ ] On a multi-image run, `SELECT COUNT(DISTINCT Id) FROM inode` equals `COUNT(*)` (Id is unique across the whole corpus).
