# Multi-Evidence File Processing Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Enable llama to process multiple evidence files (disk images) in a single invocation, producing a unified analysis database.

**Architecture:** Outer loop in `Llama::search()` iterates over evidence files sequentially. Three new DuckDB tables (`evidence_files`, `volumes`, `filesystems`) store the image/partition/filesystem hierarchy. `TskImgAssembler` is refactored from JSON output to relational record emission. Thread pool is shared across iterations; `FileScheduler` is created fresh per evidence file with a completion-signaling mechanism.

**Tech Stack:** C++20, Catch2, DuckDB, Boost (asio, program_options, pfr), Meson, TSK

**Build/Test commands:**
```bash
meson compile -C builddir
./builddir/test/llama_test           # all tests
./builddir/test/llama_test "testName" # single test
```

**Spec:** `docs/superpowers/specs/2026-03-29-multi-evidence-processing-design.md`

---

## File Structure

| File | Action | Responsibility |
|------|--------|---------------|
| `include/evidencerec.h` | Create | EvidenceFileRec, VolumeRec, FilesystemRec structs |
| `include/inode.h` | Modify | Replace FsOffset with EvidenceFileName + ByteOffset |
| `include/options.h` | Modify | Input → Inputs vector |
| `src/cli.cpp` | Modify | Multi-input parsing, duplicate filename validation |
| `include/tskimgassembler.h` | Modify | Replace JSON with record-based output |
| `src/tskimgassembler.cpp` | Modify | Emit records instead of JSON |
| `include/progressinfo.h` | Modify | Add evidence file tracking fields |
| `src/progressinfo.cpp` | Modify | Evidence file display in formatLine |
| `include/tskreader.h` | Modify | Remove JSON dependency, update assembler usage |
| `src/tskreader.cpp` | Modify | Stamp EvidenceFileName/ByteOffset on Inode/Entry |
| `include/llama.h` | Modify | openInput returns to member, Inputs loop |
| `src/llama.cpp` | Modify | Outer loop, new table init, prefetch, completion signaling |
| `include/filescheduler.h` | Modify | Add completion signaling |
| `src/filescheduler.cpp` | Modify | Implement completion future |
| `test/test_duckdb.cpp` | Modify | Update Inode tests, add new table tests |
| `test/test_cli.cpp` | Modify | Multi-input tests, duplicate filename tests |
| `test/test_tskimgassembler.cpp` | Modify | Rewrite for record-based output |
| `test/test_progressinfo.cpp` | Modify | Evidence file display tests |
| `test/test_tskconversion.cpp` | Modify | Update Inode construction |
| `test/test_filescheduler.cpp` | No change | BucketState tests don't touch Inode/evidence |
| `test/meson.build` | No change | No new source files needed |

---

### Task 1: New Record Structs for Evidence Hierarchy Tables

**Files:**
- Create: `include/evidencerec.h`
- Test: `test/test_duckdb.cpp`

- [ ] **Step 1: Write failing test for EvidenceFileRec table creation**

Add to `test/test_duckdb.cpp`:

```cpp
#include "evidencerec.h"

TEST_CASE("evidenceFileRecTableCreation") {
  LlamaDB db;
  LlamaDBConnection conn(db);

  using DuckEvidenceFile = DBType<EvidenceFileRec>;

  static_assert(DuckEvidenceFile::ColNames.size() == 7);
  static_assert(DuckEvidenceFile::NumCols == 7);
  REQUIRE(DuckEvidenceFile::createTable(conn.get(), "evidence_files"));

  EvidenceFileRec rec{"laptop.E01", "/mnt/evidence/laptop.E01", "ewf", "Expert Witness Format", 500000000000, 512, ""};

  DBBatch<EvidenceFileRec> batch;
  batch.add(rec);
  REQUIRE(batch.size() == 1);

  LlamaDBAppender appender(conn.get(), "evidence_files");
  REQUIRE(1 == batch.copyToDB(appender.get()));
  REQUIRE(appender.flush());

  duckdb_result result;
  auto state = duckdb_query(conn.get(), "SELECT * FROM evidence_files;", &result);
  CHECK(state != DuckDBError);
  CHECK(duckdb_row_count(&result) == 1);
  REQUIRE(duckdb_column_count(&result) == 7);
  duckdb_destroy_result(&result);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `meson compile -C builddir && ./builddir/test/llama_test "evidenceFileRecTableCreation"`
Expected: Compilation fails — `evidencerec.h` doesn't exist.

- [ ] **Step 3: Create evidencerec.h with all three record structs**

Create `include/evidencerec.h`:

```cpp
#pragma once

#include <cstdint>
#include <string>

#include "llamaduck.h"

struct EvidenceFileRec {
  static constexpr auto ColNames = {"Name",
                                    "Path",
                                    "ImageType",
                                    "ImageDescription",
                                    "ImageSize",
                                    "SectorSize",
                                    "VerificationHash"};

  std::string Name;
  std::string Path;
  std::string ImageType;
  std::string ImageDescription;
  uint64_t    ImageSize;
  uint64_t    SectorSize;
  std::string VerificationHash;
};

using EvidenceFileBatch = DBBatch<EvidenceFileRec>;

struct VolumeRec {
  static constexpr auto ColNames = {"EvidenceFileName",
                                    "Addr",
                                    "TableNum",
                                    "Description",
                                    "Flags",
                                    "NumBlocks",
                                    "SlotNum",
                                    "StartBlock",
                                    "VsType",
                                    "VsDescription",
                                    "VsBlockSize"};

  std::string EvidenceFileName;
  uint64_t    Addr;
  uint64_t    TableNum;
  std::string Description;
  std::string Flags;
  uint64_t    NumBlocks;
  uint64_t    SlotNum;
  uint64_t    StartBlock;
  std::string VsType;
  std::string VsDescription;
  uint64_t    VsBlockSize;
};

using VolumeBatch = DBBatch<VolumeRec>;

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
                                    "RootDirentId"};

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
};

using FilesystemBatch = DBBatch<FilesystemRec>;
```

- [ ] **Step 4: Run test to verify it passes**

Run: `meson compile -C builddir && ./builddir/test/llama_test "evidenceFileRecTableCreation"`
Expected: PASS

- [ ] **Step 5: Write tests for VolumeRec and FilesystemRec**

Add to `test/test_duckdb.cpp`:

```cpp
TEST_CASE("volumeRecTableCreation") {
  LlamaDB db;
  LlamaDBConnection conn(db);

  using DuckVolume = DBType<VolumeRec>;

  static_assert(DuckVolume::ColNames.size() == 11);
  REQUIRE(DuckVolume::createTable(conn.get(), "volumes"));

  VolumeRec rec{"laptop.E01", 0, 0, "NTFS (0x07)", "Allocated", 1024000, 0, 2048, "GPT", "GUID Partition Table", 512};

  DBBatch<VolumeRec> batch;
  batch.add(rec);
  LlamaDBAppender appender(conn.get(), "volumes");
  REQUIRE(1 == batch.copyToDB(appender.get()));
  REQUIRE(appender.flush());

  duckdb_result result;
  duckdb_query(conn.get(), "SELECT count(*) FROM volumes", &result);
  REQUIRE(duckdb_value_int64(&result, 0, 0) == 1);
  duckdb_destroy_result(&result);
}

TEST_CASE("filesystemRecTableCreation") {
  LlamaDB db;
  LlamaDBConnection conn(db);

  using DuckFs = DBType<FilesystemRec>;

  static_assert(DuckFs::ColNames.size() == 20);
  REQUIRE(DuckFs::createTable(conn.get(), "filesystems"));

  FilesystemRec rec{"laptop.E01", 1048576, 0, 0, "ntfs", 4096, 262144, 512, "Cluster", 1, 0, 0, 262143, 65535, "", "ABCDEF1234", 0, 5, 65536, ""};

  DBBatch<FilesystemRec> batch;
  batch.add(rec);
  LlamaDBAppender appender(conn.get(), "filesystems");
  REQUIRE(1 == batch.copyToDB(appender.get()));
  REQUIRE(appender.flush());

  duckdb_result result;
  duckdb_query(conn.get(), "SELECT count(*) FROM filesystems", &result);
  REQUIRE(duckdb_value_int64(&result, 0, 0) == 1);
  duckdb_destroy_result(&result);
}
```

- [ ] **Step 6: Run all new tests**

Run: `meson compile -C builddir && ./builddir/test/llama_test "evidenceFileRecTableCreation,volumeRecTableCreation,filesystemRecTableCreation"`
Expected: All PASS

- [ ] **Step 7: Commit**

```bash
git add include/evidencerec.h test/test_duckdb.cpp
git commit -m "feat: add evidence_files, volumes, filesystems record structs"
```

---

### Task 2: Inode Schema Change

Replace `FsOffset` (uint64_t) with `EvidenceFileName` (string) + `ByteOffset` (uint64_t) on the Inode struct. This changes the column count from 15 to 16.

**Files:**
- Modify: `include/inode.h`
- Modify: `test/test_duckdb.cpp` (inodeWriting test)
- Modify: `src/tskconversion.cpp:391-407` (convertMetaToInode)
- Modify: `src/tskreader.cpp:102-104` (addToBatch stamps inode.FsOffset)
- Modify: `src/filescheduler.cpp:63-91` (writeDirentsAndInodes INSERT)

- [ ] **Step 1: Update the inodeWriting test for new schema**

In `test/test_duckdb.cpp`, update the `inodeWriting` test. The Inode initializer lists change — `FsOffset` position becomes two fields: `EvidenceFileName` (string) then `ByteOffset` (uint64_t).

```cpp
TEST_CASE("inodeWriting") {
  LlamaDB db;
  LlamaDBConnection conn(db);

  using DuckInode = DBType<Inode>;

  static_assert(DuckInode::ColNames.size() == 16);
  static_assert(DuckInode::colIndex("Id") == 0);
  static_assert(DuckInode::colIndex("EvidenceFileName") == 4);
  static_assert(DuckInode::colIndex("ByteOffset") == 5);
  static_assert(DuckInode::colIndex("Modified") == 14);
  REQUIRE(DuckInode::createTable(conn.get(), "inode"));

  Inode i1{"id 1", "File", "Allocated", 16, "laptop.E01", 32768, 12345, 500, 1000, "", 1, 37, "1978-04-01 12:32:25", "2024-08-22 14:45:00", "2024-08-22 22:42:23", "2024-07-13 02:12:59"};
  Inode i2{"id 2", "File", "Deleted", 17, "laptop.E01", 32768, 987654321098765432u, 501, 1001, "", 2, 38, "1978-04-01 12:32:25", "2024-08-22 14:45:00", "2024-08-22 22:42:23", "2024-07-13 02:12:59"};

  InodeBatch batch;
  batch.add(i1);
  batch.add(i2);
  REQUIRE(batch.size() == 2);

  LlamaDBAppender appender(conn.get(), "inode");
  REQUIRE(2 == batch.copyToDB(appender.get()));
  REQUIRE(appender.flush());

  duckdb_result result;
  auto state = duckdb_query(conn.get(), "SELECT * FROM inode;", &result);
  CHECK(state != DuckDBError);
  CHECK(duckdb_row_count(&result) == 2);
  REQUIRE(duckdb_column_count(&result) == DuckInode::ColNames.size());
  unsigned int i = 0;
  REQUIRE(std::string("Id") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("Type") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("Flags") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("Addr") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("EvidenceFileName") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("ByteOffset") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("Filesize") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("Uid") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("Gid") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("LinkTarget") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("NumLinks") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("SeqNum") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("Created") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("Accessed") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("Modified") == duckdb_column_name(&result, i++));
  REQUIRE(std::string("Metadata") == duckdb_column_name(&result, i));
  duckdb_destroy_result(&result);

  state = duckdb_query(conn.get(), "SELECT * FROM inode WHERE inode.id = 'id 1';", &result);
  CHECK(state != DuckDBError);
  CHECK(duckdb_row_count(&result) == 1);
  duckdb_destroy_result(&result);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `meson compile -C builddir && ./builddir/test/llama_test "inodeWriting"`
Expected: Compilation error or assertion failure — Inode struct still has old layout.

- [ ] **Step 3: Update Inode struct**

In `include/inode.h`, replace `FsOffset` with `EvidenceFileName` + `ByteOffset`:

```cpp
struct Inode {

  static constexpr auto ColNames = {"Id",
                                    "Type",
                                    "Flags",
                                    "Addr",
                                    "EvidenceFileName",
                                    "ByteOffset",
                                    "Filesize",
                                    "Uid",
                                    "Gid",
                                    "LinkTarget",
                                    "NumLinks",
                                    "SeqNum",
                                    "Created",
                                    "Accessed",
                                    "Modified",
                                    "Metadata"};

  std::string Id;

  std::string Type;
  std::string Flags;

  uint64_t Addr;
  std::string EvidenceFileName;
  uint64_t ByteOffset;
  uint64_t Filesize;
  uint64_t Uid;
  uint64_t Gid;

  std::string LinkTarget;
  uint64_t    NumLinks;
  uint64_t    SeqNum;

  std::string Created;
  std::string Accessed;
  std::string Modified;
  std::string Metadata;
};
```

- [ ] **Step 4: Fix compilation errors in tskconversion.cpp**

In `src/tskconversion.cpp:391-407`, `convertMetaToInode` sets `n.FsOffset` — but that no longer exists. Remove the `FsOffset` assignment from this function. The caller (`tskreader.cpp:addToBatch`) will set both `EvidenceFileName` and `ByteOffset`.

Change `convertMetaToInode` — simply remove any reference to `FsOffset`. The function currently doesn't set it anyway (it's set in `tskreader.cpp:104`). Verify there's no reference to `FsOffset` in `convertMetaToInode`. If `n.FsOffset = ...` doesn't appear in `convertMetaToInode`, no change needed here.

- [ ] **Step 5: Fix tskreader.cpp — stamp EvidenceFileName and ByteOffset**

In `src/tskreader.cpp:102-104`, change:

```cpp
    inode.FsOffset = CurFsOffset;
```

to:

```cpp
    inode.EvidenceFileName = std::filesystem::path(ImgPath).filename().string();
    inode.ByteOffset = CurFsOffset;
```

Add `#include <filesystem>` at the top of `tskreader.cpp` if not already present.

- [ ] **Step 6: Fix any remaining compilation errors**

Search for all references to `FsOffset` on Inode objects and update. Key locations:
- `src/filescheduler.cpp:63-91`: The `writeDirentsAndInodes` function uses temp table + INSERT. The `INSERT INTO inode SELECT * FROM _temp_inode` pattern means the temp table schema must match. This should work automatically since it's generated from the same struct. No change needed.
- Any test files that construct Inode objects directly — update their initializer lists to include EvidenceFileName (string) before ByteOffset (uint64_t).

Check `test/test_tskconversion.cpp` for Inode construction and update if needed.

- [ ] **Step 7: Run all tests to verify**

Run: `meson compile -C builddir && ./builddir/test/llama_test`
Expected: All tests pass. Fix any remaining compilation issues.

- [ ] **Step 8: Commit**

```bash
git add include/inode.h src/tskconversion.cpp src/tskreader.cpp test/test_duckdb.cpp
git add -u  # catch any other modified files
git commit -m "feat: replace Inode.FsOffset with EvidenceFileName + ByteOffset"
```

---

### Task 3: CLI Changes — Multiple Inputs

**Files:**
- Modify: `include/options.h`
- Modify: `src/cli.cpp`
- Modify: `test/test_cli.cpp`

- [ ] **Step 1: Write failing tests for multi-input CLI**

Add to `test/test_cli.cpp`:

```cpp
TEST_CASE("testCLIMultipleInputs") {
  const char* args[] = {"llama", "output", "image1.E01", "usb.dd", "phone.img"};
  Cli cli;
  auto opts = cli.parse(5, args);
  REQUIRE("search" == opts->Command);
  REQUIRE("output" == opts->Output);
  REQUIRE(opts->Inputs.size() == 3);
  REQUIRE("image1.E01" == opts->Inputs[0]);
  REQUIRE("usb.dd" == opts->Inputs[1]);
  REQUIRE("phone.img" == opts->Inputs[2]);
}

TEST_CASE("testCLISingleInputBackcompat") {
  const char* args[] = {"llama", "output", "nosnits_workstation.E01"};
  Cli cli;
  auto opts = cli.parse(3, args);
  REQUIRE("search" == opts->Command);
  REQUIRE(opts->Inputs.size() == 1);
  REQUIRE("nosnits_workstation.E01" == opts->Inputs[0]);
}

TEST_CASE("testCLIDuplicateFilenameRejection") {
  const char* args[] = {"llama", "output", "/path/a/image.E01", "/path/b/image.E01"};
  Cli cli;
  CHECK_THROWS(cli.parse(4, args));
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `meson compile -C builddir && ./builddir/test/llama_test "testCLIMultipleInputs"`
Expected: Compilation error — `opts->Inputs` doesn't exist.

- [ ] **Step 3: Update Options struct**

In `include/options.h`, change `Input` to `Inputs`:

```cpp
struct Options {
  std::string Command;
  std::vector<std::string> Inputs;
  std::string Output;
  std::string RuleFile;
  std::string RuleDir;
  std::string MatchSet;
  std::string ExclusionHashset;
  std::string InclusionHashset;
  std::string SignaturesPath;
  std::vector<std::string> KeyFiles;
  unsigned int NumThreads;
  Codec OutputCodec;
};
```

- [ ] **Step 4: Update cli.cpp**

In `src/cli.cpp`:

1. Change the `"input"` option from `po::value<std::string>` to `po::value<std::vector<std::string>>`:

```cpp
    ("input", po::value<std::vector<std::string>>(&Opts->Inputs), "Evidence file(s) or directory(s) to process")
```

2. Update `figureOutCommand()` — change `optsMap.count("input") == 0` to check `Inputs.empty()`:

```cpp
  if (Opts->Inputs.empty()) {
    throw std::invalid_argument("No input file/directory was specified");
  }
```

3. Update `printHelp()`:

```cpp
  out << "\nUsage: llama [OPTIONS] OUTPUT_DIRECTORY INPUT_FILE [INPUT_FILE ...]\n"
```

4. Add duplicate filename validation to `validateOpts()`:

```cpp
void Cli::validateOpts() const {
  // ... existing rule file/dir validation ...

  // Check for duplicate evidence filenames
  std::set<std::string> filenames;
  for (const auto& input : Opts->Inputs) {
    std::string name = std::filesystem::path(input).filename().string();
    if (!filenames.insert(name).second) {
      throw std::invalid_argument("Duplicate evidence filename: " + name);
    }
  }
}
```

Add `#include <set>` at the top of cli.cpp.

- [ ] **Step 5: Fix all references to Opts->Input**

Search the codebase for `Opts->Input` and update to `Opts->Inputs[0]` or loop as appropriate. Key locations:
- `src/llama.cpp:293` — `openInput(this->Opts->Input)` → will be changed in Task 7 to loop, but for now change to `openInput(this->Opts->Inputs[0])` to maintain compilation.

- [ ] **Step 6: Update existing CLI tests**

In `test/test_cli.cpp`, update tests that reference `opts->Input`:
- `testCLIDefaultCommand`: `opts->Input` → `opts->Inputs[0]`
- `testCLIReal`: `opts->Input` → `opts->Inputs[0]`

- [ ] **Step 7: Run all tests**

Run: `meson compile -C builddir && ./builddir/test/llama_test`
Expected: All tests pass including new multi-input tests.

- [ ] **Step 8: Commit**

```bash
git add include/options.h src/cli.cpp test/test_cli.cpp src/llama.cpp
git commit -m "feat: support multiple positional evidence file inputs"
```

---

### Task 4: ProgressInfo Evidence File Tracking

**Files:**
- Modify: `include/progressinfo.h`
- Modify: `src/progressinfo.cpp`
- Modify: `test/test_progressinfo.cpp`

- [ ] **Step 1: Write failing test for evidence file display**

Add to `test/test_progressinfo.cpp`:

```cpp
TEST_CASE("ProgressInfo setEvidenceFile resets filesystem counters") {
  ProgressInfo pi;
  pi.setEvidenceFile("laptop.E01", 1, 3);
  pi.setFilesystem(1, 0, 1000, 500ULL * 1024 * 1024);
  pi.update(100, 1024);

  REQUIRE(pi.evidenceFileName() == "laptop.E01");
  REQUIRE(pi.evidenceIndex() == 1);
  REQUIRE(pi.evidenceCount() == 3);

  // setEvidenceFile resets filesystem-level counters
  pi.setEvidenceFile("usb.dd", 2, 3);
  REQUIRE(pi.filesystemIndex() == 0);
  REQUIRE(pi.evidenceFileName() == "usb.dd");
  REQUIRE(pi.evidenceIndex() == 2);
  // Processed counters do NOT reset (cumulative across all evidence)
  REQUIRE(pi.inodesProcessed() == 100);
}

TEST_CASE("ProgressInfo formatLine shows evidence file for multiple inputs") {
  ProgressInfo pi;
  pi.setEvidenceFile("usb.dd", 2, 3);
  pi.setFilesystem(1, 2, 1000, 500ULL * 1024 * 1024);
  pi.update(420, 50ULL * 1024 * 1024);
  std::string line = pi.formatLine(10.0);
  REQUIRE(line.find("[2/3] usb.dd") != std::string::npos);
  REQUIRE(line.find("Filesystem 1/2") != std::string::npos);
}

TEST_CASE("ProgressInfo formatLine suppresses evidence prefix for single input") {
  ProgressInfo pi;
  pi.setEvidenceFile("laptop.E01", 1, 1);
  pi.setFilesystem(1, 2, 1000, 500ULL * 1024 * 1024);
  pi.update(420, 50ULL * 1024 * 1024);
  std::string line = pi.formatLine(10.0);
  // Should NOT show "[1/1]" prefix
  REQUIRE(line.find("[1/1]") == std::string::npos);
  REQUIRE(line.find("laptop.E01") == std::string::npos);
  // Should still show filesystem info
  REQUIRE(line.find("Filesystem 1/2") != std::string::npos);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `meson compile -C builddir && ./builddir/test/llama_test "ProgressInfo setEvidenceFile"`
Expected: Compilation error — `setEvidenceFile` doesn't exist.

- [ ] **Step 3: Update ProgressInfo header**

In `include/progressinfo.h`, add the new fields and methods:

```cpp
class ProgressInfo {
public:
  ProgressInfo();

  void update(uint64_t inodes, uint64_t bytes);
  void setFilesystem(uint32_t index, uint32_t total, uint64_t inodeCount, uint64_t totalBytes);
  void setEvidenceFile(const std::string& name, uint32_t index, uint32_t total);
  void setDone();
  void addException();

  uint64_t inodesProcessed() const;
  uint64_t bytesProcessed() const;
  uint32_t filesystemIndex() const;
  uint32_t filesystemCount() const;
  uint64_t inodeCount() const;
  uint64_t totalBytes() const;
  bool isDone() const;
  uint64_t exceptionCount() const;

  std::string evidenceFileName() const;
  uint32_t evidenceIndex() const;
  uint32_t evidenceCount() const;

  std::string formatLine(double elapsedSecs) const;

private:
  std::atomic<uint64_t> InodesProcessed;
  std::atomic<uint64_t> BytesProcessed;
  std::atomic<uint32_t> FilesystemIndex;
  std::atomic<uint32_t> FilesystemCount;
  std::atomic<uint64_t> InodeCount;
  std::atomic<uint64_t> TotalBytes;
  std::atomic<bool> Done;
  std::atomic<uint64_t> ExceptionCount;

  std::string EvidenceFileName;
  std::atomic<uint32_t> EvidenceIndex;
  std::atomic<uint32_t> EvidenceCount;
};
```

Note: `EvidenceFileName` is not atomic — it's only written by the main thread before filesystem walking begins, so no race.

- [ ] **Step 4: Implement setEvidenceFile and update formatLine**

In `src/progressinfo.cpp`:

Add the constructor init for new fields:
```cpp
ProgressInfo::ProgressInfo()
  : InodesProcessed(0), BytesProcessed(0),
    FilesystemIndex(0), FilesystemCount(0),
    InodeCount(0), TotalBytes(0), Done(false), ExceptionCount(0),
    EvidenceFileName(), EvidenceIndex(0), EvidenceCount(0) {}
```

Add the new methods:
```cpp
void ProgressInfo::setEvidenceFile(const std::string& name, uint32_t index, uint32_t total) {
  EvidenceFileName = name;
  EvidenceIndex.store(index);
  EvidenceCount.store(total);
  FilesystemIndex.store(0);
  FilesystemCount.store(0);
}

std::string ProgressInfo::evidenceFileName() const {
  return EvidenceFileName;
}

uint32_t ProgressInfo::evidenceIndex() const {
  return EvidenceIndex.load();
}

uint32_t ProgressInfo::evidenceCount() const {
  return EvidenceCount.load();
}
```

Update `formatLine()` — prepend evidence file prefix when `EvidenceCount > 1`:

```cpp
std::string ProgressInfo::formatLine(double elapsedSecs) const {
  // ... existing local variable declarations ...
  uint32_t evIdx = evidenceIndex();
  uint32_t evCount = evidenceCount();

  char buf[512];
  char* p = buf;
  char* end = buf + sizeof(buf);

  // Evidence file prefix (only for multiple inputs)
  if (evCount > 1) {
    p += std::snprintf(p, end - p, "[%u/%u] %s | ",
                       evIdx, evCount, EvidenceFileName.c_str());
  }

  // Filesystem label (existing code)
  if (fsCount > 0) {
    p += std::snprintf(p, end - p, "Filesystem %u/%u | ", fsIdx, fsCount);
  }
  // ... rest unchanged ...
```

Increase `buf` from 256 to 512 to accommodate the evidence file prefix.

- [ ] **Step 5: Run all progress tests**

Run: `meson compile -C builddir && ./builddir/test/llama_test "[progressinfo],[ProgressInfo]"`
Or: `./builddir/test/llama_test "ProgressInfo"`
Expected: All pass.

- [ ] **Step 6: Commit**

```bash
git add include/progressinfo.h src/progressinfo.cpp test/test_progressinfo.cpp
git commit -m "feat: add evidence file tracking to ProgressInfo"
```

---

### Task 5: Refactor TskImgAssembler to Emit Records

Replace JSON output with the new record structs. Keep the state machine.

**Files:**
- Modify: `include/tskimgassembler.h`
- Modify: `src/tskimgassembler.cpp`
- Modify: `test/test_tskimgassembler.cpp`

- [ ] **Step 1: Write failing test for record-based assembler**

Replace the contents of `test/test_tskimgassembler.cpp` with a record-based test:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <stdexcept>

#include "tskimgassembler.h"
#include "evidencerec.h"

TEST_CASE("assemblerImgVolumeSystemVolumeFS") {
  TskImgAssembler a;

  a.addImage("laptop.E01", "/mnt/evidence/laptop.E01", "ewf", "Expert Witness Format", 500000000000, 512, "");

  a.addVolumeSystem("GPT", "GUID Partition Table", 512);

  a.addVolume(0, 0, "NTFS (0x07)", "Allocated", 1024000, 0, 2048);

  a.addFileSystem(1048576, "ntfs", 4096, 262144, 512, "Cluster", true,
                  0, 0, 262143, 65535, "", "ABCDEF", 0, 5, 65536);

  a.addVolume(1, 0, "Linux (0x83)", "Allocated", 2048000, 1, 1026048);

  auto& evidence = a.evidenceFile();
  REQUIRE(evidence.Name == "laptop.E01");
  REQUIRE(evidence.ImageType == "ewf");
  REQUIRE(evidence.ImageSize == 500000000000);

  auto& volumes = a.volumes();
  REQUIRE(volumes.size() == 2);
  REQUIRE(volumes[0].Addr == 0);
  REQUIRE(volumes[0].VsType == "GPT");
  REQUIRE(volumes[1].Description == "Linux (0x83)");

  auto& filesystems = a.filesystems();
  REQUIRE(filesystems.size() == 1);
  REQUIRE(filesystems[0].ByteOffset == 1048576);
  REQUIRE(filesystems[0].Type == "ntfs");
  REQUIRE(filesystems[0].EvidenceFileName == "laptop.E01");
  REQUIRE(filesystems[0].VolumeAddr == 0);
}

TEST_CASE("assemblerImgFS") {
  TskImgAssembler a;

  a.addImage("raw.dd", "/mnt/raw.dd", "raw", "Single raw file", 1000000, 512, "");
  a.addFileSystem(0, "ext4", 4096, 100000, 512, "Block", false,
                  0, 2, 99999, 10000, "", "1234ABCD", 8, 2, 10001);

  auto& evidence = a.evidenceFile();
  REQUIRE(evidence.Name == "raw.dd");

  auto& filesystems = a.filesystems();
  REQUIRE(filesystems.size() == 1);
  REQUIRE(filesystems[0].ByteOffset == 0);
  REQUIRE(filesystems[0].VolumeAddr == 0);
  REQUIRE(filesystems[0].VolumeTableNum == 0);
}

TEST_CASE("assemblerIllegalTransitionInitToVS") {
  TskImgAssembler a;
  CHECK_THROWS_AS(a.addVolumeSystem("GPT", "", 512), std::runtime_error);
}

TEST_CASE("assemblerIllegalTransitionInitToFS") {
  TskImgAssembler a;
  CHECK_THROWS_AS(a.addFileSystem(0, "ntfs", 4096, 100, 512, "", true, 0, 0, 99, 99, "", "", 0, 5, 100), std::runtime_error);
}

TEST_CASE("assemblerIllegalTransitionImgToVol") {
  TskImgAssembler a;
  a.addImage("test.dd", "/test.dd", "raw", "", 100, 512, "");
  CHECK_THROWS_AS(a.addVolume(0, 0, "", "", 0, 0, 0), std::runtime_error);
}

TEST_CASE("assemblerIllegalTransitionImgFSToFS") {
  TskImgAssembler a;
  a.addImage("test.dd", "/test.dd", "raw", "", 100, 512, "");
  a.addFileSystem(0, "ext4", 4096, 100, 512, "", false, 0, 0, 99, 99, "", "", 0, 2, 100);
  CHECK_THROWS_AS(a.addFileSystem(1024, "ntfs", 4096, 100, 512, "", true, 0, 0, 99, 99, "", "", 0, 5, 100), std::runtime_error);
}

TEST_CASE("assemblerCurrentFsInfo") {
  TskImgAssembler a;
  a.addImage("laptop.E01", "/mnt/laptop.E01", "ewf", "", 500000000000, 512, "");
  a.addVolumeSystem("GPT", "", 512);
  a.addVolume(0, 0, "", "", 0, 0, 0);
  a.addFileSystem(1048576, "ntfs", 4096, 262144, 512, "", true, 0, 0, 262143, 65535, "", "", 0, 5, 65536);

  REQUIRE(a.currentEvidenceFileName() == "laptop.E01");
  REQUIRE(a.currentByteOffset() == 1048576);
  REQUIRE(a.fsIndex() == 1);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `meson compile -C builddir && ./builddir/test/llama_test "assemblerImgVolumeSystemVolumeFS"`
Expected: Compilation error — TskImgAssembler API has changed.

- [ ] **Step 3: Rewrite tskimgassembler.h**

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "evidencerec.h"

class TskImgAssembler {
public:
  void addImage(const std::string& name, const std::string& path,
                const std::string& type, const std::string& description,
                uint64_t size, uint64_t sectorSize,
                const std::string& verificationHash);

  void addVolumeSystem(const std::string& type, const std::string& description,
                       uint64_t blockSize);

  void addVolume(uint64_t addr, uint64_t tableNum,
                 const std::string& description, const std::string& flags,
                 uint64_t numBlocks, uint64_t slotNum, uint64_t startBlock);

  void addFileSystem(uint64_t byteOffset, const std::string& type,
                     uint64_t blockSize, uint64_t blockCount,
                     uint64_t deviceBlockSize, const std::string& blockName,
                     bool littleEndian,
                     uint64_t firstBlock, uint64_t firstInum,
                     uint64_t lastBlock, uint64_t lastInum,
                     const std::string& flags, const std::string& fsID,
                     uint64_t journalInum, uint64_t rootInum,
                     uint64_t numInums);

  const EvidenceFileRec& evidenceFile() const { return Evidence; }
  const std::vector<VolumeRec>& volumes() const { return Volumes; }
  const std::vector<FilesystemRec>& filesystems() const { return Filesystems; }

  std::string currentEvidenceFileName() const { return Evidence.Name; }
  uint64_t currentByteOffset() const { return CurByteOffset; }
  uint32_t fsIndex() const { return FsIdx; }

private:
  enum { INIT, IMG, IMG_FS, VS, VOL, VOL_FS } State = INIT;

  EvidenceFileRec Evidence;
  std::vector<VolumeRec> Volumes;
  std::vector<FilesystemRec> Filesystems;

  // Current volume system context (stamped onto volumes)
  std::string CurVsType;
  std::string CurVsDescription;
  uint64_t CurVsBlockSize = 0;

  // Current volume context (stamped onto filesystems)
  uint64_t CurVolAddr = 0;
  uint64_t CurVolTableNum = 0;

  // Current filesystem tracking
  uint64_t CurByteOffset = 0;
  uint32_t FsIdx = 0;
};
```

- [ ] **Step 4: Rewrite tskimgassembler.cpp**

```cpp
#include "tskimgassembler.h"
#include "throw.h"

void TskImgAssembler::addImage(const std::string& name, const std::string& path,
                                const std::string& type, const std::string& description,
                                uint64_t size, uint64_t sectorSize,
                                const std::string& verificationHash) {
  THROW_IF(State != INIT, "Attempted to add image in state " << State);
  Evidence.Name = name;
  Evidence.Path = path;
  Evidence.ImageType = type;
  Evidence.ImageDescription = description;
  Evidence.ImageSize = size;
  Evidence.SectorSize = sectorSize;
  Evidence.VerificationHash = verificationHash;
  State = IMG;
}

void TskImgAssembler::addVolumeSystem(const std::string& type,
                                       const std::string& description,
                                       uint64_t blockSize) {
  THROW_IF(State != IMG, "Attempted to add volume system in state " << State);
  CurVsType = type;
  CurVsDescription = description;
  CurVsBlockSize = blockSize;
  State = VS;
}

void TskImgAssembler::addVolume(uint64_t addr, uint64_t tableNum,
                                 const std::string& description,
                                 const std::string& flags,
                                 uint64_t numBlocks, uint64_t slotNum,
                                 uint64_t startBlock) {
  switch (State) {
  case VS:
  case VOL:
  case VOL_FS:
    break;
  default:
    THROW("Attempted to add volume in state " << State);
  }

  VolumeRec vol;
  vol.EvidenceFileName = Evidence.Name;
  vol.Addr = addr;
  vol.TableNum = tableNum;
  vol.Description = description;
  vol.Flags = flags;
  vol.NumBlocks = numBlocks;
  vol.SlotNum = slotNum;
  vol.StartBlock = startBlock;
  vol.VsType = CurVsType;
  vol.VsDescription = CurVsDescription;
  vol.VsBlockSize = CurVsBlockSize;
  Volumes.push_back(std::move(vol));

  CurVolAddr = addr;
  CurVolTableNum = tableNum;
  State = VOL;
}

void TskImgAssembler::addFileSystem(uint64_t byteOffset, const std::string& type,
                                     uint64_t blockSize, uint64_t blockCount,
                                     uint64_t deviceBlockSize, const std::string& blockName,
                                     bool littleEndian,
                                     uint64_t firstBlock, uint64_t firstInum,
                                     uint64_t lastBlock, uint64_t lastInum,
                                     const std::string& flags, const std::string& fsID,
                                     uint64_t journalInum, uint64_t rootInum,
                                     uint64_t numInums) {
  FilesystemRec fs;
  fs.EvidenceFileName = Evidence.Name;
  fs.ByteOffset = byteOffset;
  fs.Type = type;
  fs.BlockSize = blockSize;
  fs.BlockCount = blockCount;
  fs.DeviceBlockSize = deviceBlockSize;
  fs.BlockName = blockName;
  fs.LittleEndian = littleEndian ? 1 : 0;
  fs.FirstBlock = firstBlock;
  fs.FirstInum = firstInum;
  fs.LastBlock = lastBlock;
  fs.LastInum = lastInum;
  fs.Flags = flags;
  fs.FsID = fsID;
  fs.JournalInum = journalInum;
  fs.RootInum = rootInum;
  fs.NumInums = numInums;
  fs.RootDirentId = "";

  if (State == IMG) {
    fs.VolumeAddr = 0;
    fs.VolumeTableNum = 0;
    State = IMG_FS;
  }
  else if (State == VOL) {
    fs.VolumeAddr = CurVolAddr;
    fs.VolumeTableNum = CurVolTableNum;
    State = VOL_FS;
  }
  else {
    THROW("Attempted to add filesystem in state " << State);
  }

  CurByteOffset = byteOffset;
  ++FsIdx;
  Filesystems.push_back(std::move(fs));
}
```

- [ ] **Step 5: Update TskReader to use new assembler API**

In `src/tskreader.cpp`, update the filter callbacks. The assembler no longer takes JSON objects.

In `startReading()`:
```cpp
bool TskReader::startReading() {
  std::string name = std::filesystem::path(ImgPath).filename().string();
  const char* type_name = tsk_img_type_toname(Img->itype);
  const char* type_desc = tsk_img_type_todesc(Img->itype);
  Asm.addImage(name, ImgPath,
               type_name ? type_name : "unknown",
               type_desc ? type_desc : "Unknown image type",
               Img->size, Img->sector_size, "");
  // ... rest unchanged ...
```

In `filterVs()`:
```cpp
TSK_FILTER_ENUM TskReader::filterVs(const TSK_VS_INFO* vs_info) {
  Asm.addVolumeSystem(
    TskUtils::volumeSystemType(vs_info->vstype),
    tsk_vs_type_todesc(vs_info->vstype),
    vs_info->block_size
  );
  return TSK_FILTER_CONT;
}
```

In `filterVol()`:
```cpp
TSK_FILTER_ENUM TskReader::filterVol(const TSK_VS_PART_INFO* vs_part) {
  Asm.addVolume(
    vs_part->addr, vs_part->table_num,
    vs_part->desc,
    TskUtils::volumeFlags(vs_part->flags),
    vs_part->len, vs_part->slot_num, vs_part->start
  );
  return TSK_FILTER_CONT;
}
```

In `filterFs()`:
```cpp
TSK_FILTER_ENUM TskReader::filterFs(TSK_FS_INFO* fs_info) {
  const bool littleEndian = fs_info->endian == TSK_LIT_ENDIAN;
  Asm.addFileSystem(
    fs_info->offset,
    tsk_fs_type_toname(fs_info->ftype),
    fs_info->block_size,
    fs_info->block_count,
    fs_info->dev_bsize,
    fs_info->duname,
    littleEndian,
    fs_info->first_block, fs_info->first_inum,
    fs_info->last_block, fs_info->last_inum,
    TskUtils::filesystemFlags(fs_info->flags),
    TskUtils::filesystemID(fs_info->fs_id, fs_info->fs_id_used, littleEndian),
    fs_info->journ_inum, fs_info->root_inum,
    fs_info->inum_count
  );
  Tsg = Tsk->makeTimestampGetter(fs_info->ftype);
  CurFsOffset = fs_info->offset;
  CurFsBlockSize = fs_info->block_size;
  InodeTracker.clear();
  InodeTracker.resize(fs_info->last_inum - fs_info->first_inum + 1, false);
  if (Progress) {
    Progress->setFilesystem(Asm.fsIndex(), 0, fs_info->inum_count,
                            static_cast<uint64_t>(fs_info->block_count) * fs_info->block_size);
  }
  Input->startFilesystem(static_cast<uint64_t>(fs_info->block_count) * fs_info->block_size);
  return TSK_FILTER_CONT;
}
```

Remove the `FsIndex` member from `TskReader` — it's now tracked by the assembler.

- [ ] **Step 6: Remove JSON dependency from TskReader**

In `include/tskreader.h`:
- Remove `#include "tskimgassembler.h"` if it pulls in jsoncons (it will since we rewrote it, but check).
- Actually, the new `tskimgassembler.h` no longer includes `jsoncons_wrapper.h`, so this is automatic.
- Remove `#include "jsoncons_wrapper.h"` from `tskimgassembler.h` (already done in Step 3).
- Remove the `FsIndex` member variable from `TskReader` (line 79 of tskreader.h) since the assembler tracks it now.

- [ ] **Step 7: Run all tests**

Run: `meson compile -C builddir && ./builddir/test/llama_test`
Expected: All tests pass.

- [ ] **Step 8: Commit**

```bash
git add include/tskimgassembler.h src/tskimgassembler.cpp test/test_tskimgassembler.cpp
git add include/tskreader.h src/tskreader.cpp
git commit -m "refactor: TskImgAssembler emits relational records instead of JSON"
```

---

### Task 6: FileScheduler Completion Signaling

The thread pool can't be `join()`ed between evidence files because that terminates the threads. Add a completion mechanism so the main loop can wait for all dispatched batches to finish.

**Files:**
- Modify: `include/filescheduler.h`
- Modify: `src/filescheduler.cpp`

- [ ] **Step 1: Add completion future to FileScheduler**

In `include/filescheduler.h`, add a completion promise/future pair and an outstanding batch counter:

```cpp
#include <atomic>
#include <future>

class FileScheduler {
public:
  // ... existing ...

  // Returns a future that completes when all dispatched batches have finished.
  // Call after flushAllBuckets() and after all input has been pushed.
  std::future<void> getCompletionFuture();

private:
  // ... existing ...

  std::atomic<uint64_t> OutstandingBatches{0};
  std::promise<void> CompletionPromise;
  std::future<void> CompletionFuture;
  bool CompletionRequested = false;
};
```

- [ ] **Step 2: Implement completion signaling**

In `src/filescheduler.cpp`:

In the constructor, initialize:
```cpp
FileScheduler::FileScheduler(...)
    : DBConn(db), Pool(pool), Strand(Pool.get_executor()),
      // ... existing ...
      OutstandingBatches(0), CompletionPromise(), CompletionFuture(CompletionPromise.get_future()),
      CompletionRequested(false) {
  // ... existing processor creation ...
}
```

In `dispatchIfReady()`, increment counter when dispatching:
```cpp
void FileScheduler::dispatchIfReady() {
  while (Buckets.hasPendingBatches()) {
    std::unique_lock<std::mutex> lock(ProcMutex);
    if (Processors.empty()) {
      return;
    }
    auto proc = Processors.back();
    Processors.pop_back();
    lock.unlock();

    auto batch = Buckets.popBatch();

    BatchLog.add(BatchRec{NextBatchId++, batch.BucketIndex,
                          batch.Entries.size(), batch.TotalBytes});
    BatchLog.copyToDB(BatchAppender.get());
    BatchAppender.flush();
    BatchLog.clear();

    OutstandingBatches.fetch_add(1);
    auto entries = std::make_shared<std::vector<std::unique_ptr<Entry>>>(
      std::move(batch.Entries));
    boost::asio::post(Pool, [=, this]() {
      proc->processBatch(entries);
      this->pushProc(proc);
      if (OutstandingBatches.fetch_sub(1) == 1) {
        // Last batch done — check completion on strand
        boost::asio::post(Strand, [this]() {
          if (CompletionRequested && !Buckets.hasPendingBatches()) {
            CompletionPromise.set_value();
            CompletionRequested = false;
          }
        });
      }
    });
  }

  // Check if all work is done after dispatching
  if (CompletionRequested && OutstandingBatches.load() == 0 && !Buckets.hasPendingBatches()) {
    CompletionPromise.set_value();
    CompletionRequested = false;  // prevent double-set
  }
}
```

Add the public method:
```cpp
std::future<void> FileScheduler::getCompletionFuture() {
  // Post to strand to ensure thread-safe check of outstanding count
  boost::asio::post(Strand, [this]() {
    CompletionRequested = true;
    if (OutstandingBatches.load() == 0 && !Buckets.hasPendingBatches()) {
      CompletionPromise.set_value();
    }
  });
  return std::move(CompletionFuture);
}
```

Note: `CompletionRequested` and the promise check in `dispatchIfReady` both run on the strand, so no data race. The `OutstandingBatches` counter is atomic for the decrement from pool threads.

- [ ] **Step 3: Run all tests to verify no regression**

Run: `meson compile -C builddir && ./builddir/test/llama_test`
Expected: All tests pass. The new mechanism isn't invoked by existing code yet.

- [ ] **Step 4: Commit**

```bash
git add include/filescheduler.h src/filescheduler.cpp
git commit -m "feat: add completion signaling to FileScheduler"
```

---

### Task 7: Llama Outer Loop with New Table Init and Prefetch

Wire everything together in `Llama::search()`.

**Files:**
- Modify: `include/llama.h`
- Modify: `src/llama.cpp`

- [ ] **Step 1: Add new table creation to dbInit()**

In `src/llama.cpp`, update `dbInit()`:

```cpp
bool Llama::dbInit() {
  DBType<Dirent>::createTable(DbConn.get(), "dirent");
  DBType<Inode>::createTable(DbConn.get(), "inode");
  DBType<HashRec>::createTable(DbConn.get(), "hash");
  DBType<Extent>::createTable(DbConn.get(), "extents");
  DBType<BatchRec>::createTable(DbConn.get(), "batches");
  DBType<ExceptionRecord>::createTable(DbConn.get(), "exception_log");
  DBType<EvidenceFileRec>::createTable(DbConn.get(), "evidence_files");
  DBType<VolumeRec>::createTable(DbConn.get(), "volumes");
  DBType<FilesystemRec>::createTable(DbConn.get(), "filesystems");
  return true;
}
```

Add `#include "evidencerec.h"` to `llama.cpp`.

- [ ] **Step 2: Restructure init() — open first input in parallel**

In `src/llama.cpp`, update `init()`:

```cpp
bool Llama::init() {
  Timer initTime(&std::cerr, "Init time: ");
  auto readPats = make_future(Pool, [this]() {
    return this->Opts->KeyFiles.size() ?
           readpatterns(this->Opts->KeyFiles): true;
  });

  auto open = make_future(Pool, [this]() {
    return openInput(this->Opts->Inputs[0]);
  });

  auto db = make_future(Pool, [this]() {
    return dbInit();
  });

  auto rules = make_future(Pool, [this](){
    return (this->Opts->RuleFile.empty() || RuleEngine->read(readfile(this->Opts->RuleFile), this->Opts->RuleFile)) &&
           (this->Opts->RuleDir.empty() || readRulesFromDir(RuleEngine, this->Opts->RuleDir));
  });

  auto sigs = make_future(Pool, [this]() {
    return loadSignatures();
  });

  return readPats.get() && open.get() && db.get() && rules.get() && sigs.get();
}
```

This is the same as today but uses `Inputs[0]`.

- [ ] **Step 3: Restructure search() — outer loop with prefetch**

Rewrite `Llama::search()`:

```cpp
void Llama::search() {
  if (init()) {
    Timer searchTime(&std::cerr, "Search time: ");
    std::filesystem::path outdir(Opts->Output);
    std::filesystem::create_directories(outdir);

    RuleEngine->createTables(DbConn);

    {
      LlamaDBAppender sigAppender(DbConn.get(), "signatures");
      SigBatch sigBatch;
      for (const auto& m : SigMagics) {
        sigBatch.add(SigRec{m->Id, m->Name, m->Description});
      }
      sigBatch.copyToDB(sigAppender.get());
      sigAppender.flush();
    }

    ProgressInfo progressInfo;

    LG_ProgramOptions opts{10};
    LgProg.reset(lg_create_program(RuleEngine->buildFsm().getFsm(), &opts), lg_destroy_program);
    auto procContext = std::make_shared<ProcessorContext>(&Db, LgProg, RuleEngine, Opts->ExclusionHashset, Opts->InclusionHashset, SigMagics, SigProg, &progressInfo);
    auto protoProc = std::make_shared<Processor>(procContext);

    ProgressThread progressThread(progressInfo, isatty(STDERR_FILENO));
    progressThread.start();

    for (size_t i = 0; i < Opts->Inputs.size(); ++i) {
      // Input[i] is already open (from init() or previous iteration's prefetch)

      // Prefetch next input
      std::future<bool> nextInputFuture;
      std::shared_ptr<InputReader> nextInput;
      if (i + 1 < Opts->Inputs.size()) {
        nextInputFuture = make_future(Pool, [this, i]() {
          return openInput(this->Opts->Inputs[i + 1]);
        });
      }

      std::string evidenceName = std::filesystem::path(Opts->Inputs[i]).filename().string();
      progressInfo.setEvidenceFile(evidenceName, i + 1, Opts->Inputs.size());

      auto scheduler = std::make_shared<FileScheduler>(Db, Pool, protoProc, Opts);
      auto inh = std::shared_ptr<InputHandler>(new BatchHandler(scheduler));

      Input->setInputHandler(inh);
      Input->setProgressInfo(&progressInfo);

#ifdef __linux__
      if (auto posixReader = std::dynamic_pointer_cast<PosixReader>(Input)) {
        auto extentAppender = std::make_shared<LlamaDBAppender>(DbConn.get(), "extents");
        posixReader->setExtentAppender(extentAppender);
      }
#endif

      if (!Input->startReading()) {
        std::cerr << "startReading returned an error for " << Opts->Inputs[i] << std::endl;
      }

      // Wait for all processing to complete
      scheduler->getCompletionFuture().get();

      // Write assembler records to DB (evidence_files, volumes, filesystems)
      if (auto tskReader = dynamic_cast<TskReader*>(Input.get())) {
        writeEvidenceRecords(tskReader->getAssembler());
      }

      if (progressInfo.exceptionCount() > 0) {
        std::cerr << progressInfo.exceptionCount()
                  << " evidence I/O exceptions encountered -- see exception_log table\n";
      }

      std::cerr << "Hashing Time (" << evidenceName << "): " << scheduler->getProcessorTime() << "s\n";

      // Wait for next input to be ready
      if (nextInputFuture.valid()) {
        nextInputFuture.get();
      }
    }

    progressThread.stop();
    Pool.join();  // All evidence files processed — terminate pool threads

    RuleEngine->writeRulesToDb(DbConn);

#ifdef __linux__
    if (std::dynamic_pointer_cast<PosixReader>(Input)) {
      std::cerr << "Creating disk map from extents..." << std::endl;
      if (createDiskMap()) {
        std::string vizDir = outdir.string() + "/diskmap";
        std::cerr << "Generating disk map visualization..." << std::endl;
        generateDiskMapVisualization(vizDir);
      }
    }
#endif

    writeDB(outdir.string());
    writeReports(outdir.string());
  }
  else {
    std::cerr << "init returned false!" << std::endl;
  }
}
```

**Note:** The above is pseudocode-level — the exact implementation requires:
1. `openInput` must store the result somewhere the prefetch can access. Currently it stores in `Input` member. For prefetch, store in a local `nextInput` and swap after the current iteration.
2. `TskReader` needs a `getAssembler()` accessor.
3. A new `writeEvidenceRecords()` method on Llama to flush assembler data to DB.

- [ ] **Step 4: Update openInput for prefetch pattern**

Change `openInput` to return the reader instead of storing in `Input`:

In `include/llama.h`, change signature:
```cpp
  std::shared_ptr<InputReader> openInput(const std::string& input);
```

In `src/llama.cpp`:
```cpp
std::shared_ptr<InputReader> Llama::openInput(const std::string& input) {
  std::shared_ptr<InputReader> reader;
  if (fs::is_directory(input)) {
#ifdef __linux__
    reader = InputReader::createPosix(input);
#else
    reader = InputReader::createDir(input);
#endif
  } else {
    reader = InputReader::createTSK(input);
  }
  return reader;
}
```

Update `init()` to store the result:
```cpp
  auto open = make_future(Pool, [this]() {
    Input = openInput(this->Opts->Inputs[0]);
    return bool(Input);
  });
```

Update the search loop for prefetch:
```cpp
    for (size_t i = 0; i < Opts->Inputs.size(); ++i) {
      // Prefetch next input in background
      std::shared_ptr<InputReader> nextInput;
      std::future<bool> nextFuture;
      if (i + 1 < Opts->Inputs.size()) {
        nextFuture = make_future(Pool, [this, i, &nextInput]() {
          nextInput = openInput(this->Opts->Inputs[i + 1]);
          return bool(nextInput);
        });
      }

      // ... process Input ...

      // Swap to next input
      if (nextFuture.valid()) {
        nextFuture.get();
        Input = nextInput;
      }
    }
```

- [ ] **Step 5: Add getAssembler() to TskReader**

In `include/tskreader.h`:
```cpp
  const TskImgAssembler& getAssembler() const { return Asm; }
```

- [ ] **Step 6: Add writeEvidenceRecords() to Llama**

In `include/llama.h`:
```cpp
  void writeEvidenceRecords(const TskImgAssembler& assembler);
```

In `src/llama.cpp`:
```cpp
void Llama::writeEvidenceRecords(const TskImgAssembler& assembler) {
  // Evidence file
  {
    LlamaDBAppender appender(DbConn.get(), "evidence_files");
    EvidenceFileBatch batch;
    batch.add(assembler.evidenceFile());
    batch.copyToDB(appender.get());
    appender.flush();
  }

  // Volumes
  if (!assembler.volumes().empty()) {
    LlamaDBAppender appender(DbConn.get(), "volumes");
    VolumeBatch batch;
    for (const auto& vol : assembler.volumes()) {
      batch.add(vol);
    }
    batch.copyToDB(appender.get());
    appender.flush();
  }

  // Filesystems
  if (!assembler.filesystems().empty()) {
    LlamaDBAppender appender(DbConn.get(), "filesystems");
    FilesystemBatch batch;
    for (const auto& fs : assembler.filesystems()) {
      batch.add(fs);
    }
    batch.copyToDB(appender.get());
    appender.flush();
  }
}
```

- [ ] **Step 7: Build and verify compilation**

Run: `meson compile -C builddir`
Expected: Clean compilation.

- [ ] **Step 8: Run all tests**

Run: `./builddir/test/llama_test`
Expected: All tests pass.

- [ ] **Step 9: Commit**

```bash
git add include/llama.h src/llama.cpp include/tskreader.h
git commit -m "feat: outer loop for sequential multi-evidence processing with prefetch"
```

---

### Task 8: Report SQL Updates

Add evidence file context to all three report queries.

**Files:**
- Modify: `src/llama.cpp` (writeReports method)

- [ ] **Step 1: Update file_inventory.sql**

In `src/llama.cpp`, update the file inventory query to join through inode → filesystems → evidence_files:

```cpp
  writeFile(outdir + "/file_inventory.sql",
    "INSTALL spatial;\nLOAD spatial;\n"
    "COPY (\n"
    "  SELECT\n"
    "    ef.Name AS EvidenceFile,\n"
    "    d.Path || d.Name AS FullPath,\n"
    "    d.Name,\n"
    "    d.Type AS DirentType,\n"
    "    d.Flags AS DirentFlags,\n"
    "    i.Type AS InodeType,\n"
    "    i.Flags AS InodeFlags,\n"
    "    CAST(i.Filesize AS BIGINT) AS Filesize,\n"
    "    i.Created,\n"
    "    i.Modified,\n"
    "    i.Accessed,\n"
    "    i.Metadata AS MetadataChanged,\n"
    "    h.MD5,\n"
    "    h.SHA1,\n"
    "    h.SHA256,\n"
    "    STRING_AGG(DISTINCT s.Name, ', ' ORDER BY s.Name) AS Signatures,\n"
    "    CAST(d.MetaAddr AS BIGINT) AS MetaAddr,\n"
    "    CAST(d.ParentAddr AS BIGINT) AS ParentAddr\n"
    "  FROM '" + outdir + "/dirent.parquet' d\n"
    "  JOIN '" + outdir + "/inode.parquet' i ON d.MetaAddr = i.Addr\n"
    "  LEFT JOIN '" + outdir + "/evidence_files.parquet' ef ON i.EvidenceFileName = ef.Name\n"
    "  LEFT JOIN '" + outdir + "/hash.parquet' h ON d.MetaAddr = h.MetaAddr\n"
    "  LEFT JOIN '" + outdir + "/file_signatures.parquet' fs ON h.SHA256 = fs.FileHash\n"
    "  LEFT JOIN '" + outdir + "/signatures.parquet' s ON fs.SigId = s.Id\n"
    "  GROUP BY ALL\n"
    "  ORDER BY ef.Name, FullPath\n"
    ") TO '" + outdir + "/file_inventory.xlsx'\n"
    "WITH (FORMAT GDAL, DRIVER 'xlsx');\n"
  );
```

- [ ] **Step 2: Update rule_hits_report.sql**

```cpp
  writeFile(outdir + "/rule_hits_report.sql",
    "INSTALL spatial;\nLOAD spatial;\n"
    "COPY (\n"
    "  SELECT\n"
    "    i.EvidenceFileName AS EvidenceFile,\n"
    "    r.name AS RuleName,\n"
    "    rh.path || rh.name AS FullPath,\n"
    "    rh.name AS FileName,\n"
    "    CAST(rh.addr AS BIGINT) AS MetaAddr\n"
    "  FROM '" + outdir + "/rule_hits.parquet' rh\n"
    "  JOIN '" + outdir + "/rules.parquet' r ON rh.id = r.id\n"
    "  LEFT JOIN '" + outdir + "/inode.parquet' i ON rh.addr = i.Addr\n"
    "  ORDER BY i.EvidenceFileName, r.name, rh.path, rh.name\n"
    ") TO '" + outdir + "/rule_hits.xlsx'\n"
    "WITH (FORMAT GDAL, DRIVER 'xlsx');\n"
  );
```

- [ ] **Step 3: Update search_hits_report.sql**

```cpp
  writeFile(outdir + "/search_hits_report.sql",
    "INSTALL spatial;\nLOAD spatial;\n"
    "COPY (\n"
    "  SELECT\n"
    "    i.EvidenceFileName AS EvidenceFile,\n"
    "    r.name AS RuleName,\n"
    "    d.Path || d.Name AS FullPath,\n"
    "    d.Name AS FileName,\n"
    "    sh.pattern AS Pattern,\n"
    "    CAST(sh.start_offset AS BIGINT) AS StartOffset,\n"
    "    CAST(sh.end_offset AS BIGINT) AS EndOffset,\n"
    "    CAST(sh.length AS BIGINT) AS Length\n"
    "  FROM '" + outdir + "/search_hits.parquet' sh\n"
    "  JOIN '" + outdir + "/hash.parquet' h ON sh.file_hash = h.SHA256\n"
    "  JOIN '" + outdir + "/dirent.parquet' d ON h.MetaAddr = d.MetaAddr\n"
    "  LEFT JOIN '" + outdir + "/inode.parquet' i ON d.MetaAddr = i.Addr\n"
    "  LEFT JOIN '" + outdir + "/rules.parquet' r ON sh.rule_id = r.id\n"
    "  ORDER BY i.EvidenceFileName, r.name, d.Path, d.Name, sh.start_offset\n"
    ") TO '" + outdir + "/search_hits.xlsx'\n"
    "WITH (FORMAT GDAL, DRIVER 'xlsx');\n"
  );
```

- [ ] **Step 4: Build and run tests**

Run: `meson compile -C builddir && ./builddir/test/llama_test`
Expected: All pass. Report SQL is generated, not executed, so no runtime test needed beyond compilation.

- [ ] **Step 5: Commit**

```bash
git add src/llama.cpp
git commit -m "feat: add evidence file context to report SQL queries"
```

---

### Task 9: Final Integration — Build Verification

Verify everything compiles and all tests pass with the complete change set.

- [ ] **Step 1: Clean build**

```bash
meson compile -C builddir --clean && meson compile -C builddir
```

- [ ] **Step 2: Run full test suite**

```bash
./builddir/test/llama_test
```

Expected: All tests pass.

- [ ] **Step 3: Verify new tables in a quick smoke test**

```bash
./builddir/test/llama_test "evidenceFileRecTableCreation,volumeRecTableCreation,filesystemRecTableCreation,inodeWriting,assemblerImgVolumeSystemVolumeFS,assemblerImgFS,testCLIMultipleInputs,testCLIDuplicateFilenameRejection"
```

Expected: All specifically-named tests pass.

- [ ] **Step 4: Commit any remaining fixes**

If any tests needed fixes, commit them:

```bash
git add -u
git commit -m "fix: address integration issues from multi-evidence implementation"
```
