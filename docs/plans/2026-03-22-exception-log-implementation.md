# Evidence Exception Handling Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Handle unreadable evidence files gracefully by logging them to a DuckDB exception_log table, showing a live count in the progress bar, and continuing processing.

**Architecture:** A new `EvidenceIOError` exception type distinguishes evidence I/O failures (normal forensic conditions) from application bugs. ReadSeekTSK and ReadSeekFile throw `EvidenceIOError` instead of `std::runtime_error`. Processor catches it per-operation, logs to an exception_log DuckDB table via the existing batch appender pattern, increments an atomic counter on ProgressInfo, and continues. Entry is enriched with evidence context (image path, filesystem, path, flags) so the exception log has full detail.

**Tech Stack:** C++, DuckDB, Catch2, The Sleuth Kit (libtsk)

**Design doc:** `docs/plans/2026-03-22-exception-log-design.md`

**Key existing patterns to follow:**
- DB record struct: see `include/ducksig.h` (SigRec, FileSigResult) — struct with `static constexpr auto ColNames` and matching fields
- Batch appender: see `include/processor.h:83-95` — LlamaDBAppender + DBBatch<T> pair, flushed in `Processor::flush()`
- Table creation: see `src/llama.cpp:214-220` — `DBType<T>::createTable(DbConn.get(), "table_name")`
- TSK flag conversion: `TskUtils::metaFlags(unsigned int flags)` in `src/tskconversion.cpp:123-133` already exists

**Timestamp note:** The `duckdbType` template (`include/llamaduck.h:72-83`) only maps `std::string` → VARCHAR and integral types → UBIGINT. There's no timestamp type support. We store the timestamp as a VARCHAR in ISO 8601 format. This is fine for a low-volume exception log.

---

### Task 1: Create EvidenceIOError exception type

A new exception class that inherits `std::runtime_error`. This lets Processor distinguish evidence I/O errors from application bugs.

**Files:**
- Create: `include/evidenceioerror.h`

**Step 1: Write a test that throws and catches EvidenceIOError**

Add a new test file. But this is trivially testable inline — the real test is that ReadSeekTSK throws it (Task 3). For now, just create the header.

**Step 2: Create the header**

Create `include/evidenceioerror.h`:

```cpp
// ABOUTME: Exception type for evidence I/O failures (unreadable files, corrupt data)
// ABOUTME: Distinguished from application errors so Processor can catch and log them

#pragma once

#include <stdexcept>

class EvidenceIOError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};
```

**Step 3: Commit**

```bash
git add include/evidenceioerror.h
git commit -m "Add EvidenceIOError exception type for evidence I/O failures

Inherits std::runtime_error. Distinguishes evidence I/O problems
(unreadable files, corrupt data) from application bugs, so Processor
can catch and log them while letting real errors propagate."
```

---

### Task 2: Add THROW_EVIDENCE_IF macro

The existing `THROW_IF` macro in `include/throw.h` throws `std::runtime_error`. We need a parallel macro that throws `EvidenceIOError`. ReadSeekTSK and ReadSeekFile will use it instead of `THROW_IF`.

**Files:**
- Modify: `include/throw.h`

**Step 1: Add the macro**

Add after the existing `THROW_IF` macro in `include/throw.h`:

```cpp
#include "evidenceioerror.h"

#define THROW_EVIDENCE(msg) \
  throw EvidenceIOError(static_cast<const std::ostringstream&>( \
                          std::ostringstream() << __FILE__ << ":" << __LINE__ << ": " << msg) \
                          .str())

#define THROW_EVIDENCE_IF(condition, msg) \
  if (condition) \
  THROW_EVIDENCE(msg)
```

**Step 2: Run tests**

Run: `make -j8 check`

Expected: All tests pass (no usage yet).

**Step 3: Commit**

```bash
git add include/throw.h
git commit -m "Add THROW_EVIDENCE_IF macro for evidence I/O errors

Parallels THROW_IF but throws EvidenceIOError instead of
std::runtime_error."
```

---

### Task 3: TDD — Make ReadSeekTSK throw EvidenceIOError

Change ReadSeekTSK to use `THROW_EVIDENCE_IF` instead of `THROW_IF` for `tsk_fs_file_read` failures.

**Files:**
- Modify: `test/test_readseek.cpp`
- Modify: `src/readseek_impl.cpp`

**Step 1: Write a failing test**

The test should verify that ReadSeekTSK throws `EvidenceIOError` (not just `std::runtime_error`) when it can't read. We can test this with an invalid inode number that causes `tsk_fs_file_read` to fail. But first we need to verify there's such a scenario with the test image.

Actually, the simplest approach: open a valid inode, then test that the exception type is correct by using `REQUIRE_THROWS_AS`. We need an inode that will fail to read. Inode 0 ($MFT) in the test image should be readable, but we can try a very large invalid inode that won't exist.

Better approach: we already know ReadSeekTSK throws when `tsk_fs_file_read` returns negative. We can't easily make that happen with our small test image. Instead, test that when we catch the exception, it IS an `EvidenceIOError` and IS-A `std::runtime_error`. We do this by changing the throw and verifying the existing tests still pass (they catch nothing, so they're unaffected), then adding a specific type-check test.

Add to `test/test_readseek.cpp`:

```cpp
#include "evidenceioerror.h"

TEST_CASE("readSeekTSK_throwsEvidenceIOError") {
  // EvidenceIOError should inherit from std::runtime_error
  EvidenceIOError err("test error");
  REQUIRE(dynamic_cast<const std::runtime_error*>(&err) != nullptr);
  REQUIRE(std::string(err.what()).find("test error") != std::string::npos);
}
```

This is a minimal type-check test. The real integration test happens when we run against evidence with unreadable files.

**Step 2: Run tests — should pass (EvidenceIOError exists from Task 1)**

Run: `make -j8 check`

Expected: All tests pass.

**Step 3: Change ReadSeekTSK to throw EvidenceIOError**

In `src/readseek_impl.cpp`, change the two `THROW_IF(bytesRead < 0, ...)` calls in ReadSeekTSK::read() to `THROW_EVIDENCE_IF(bytesRead < 0, ...)`:

Vector overload (around line 112):
```cpp
  THROW_EVIDENCE_IF(bytesRead < 0, "tsk_fs_file_read() failed for inode " << Inum);
```

Raw pointer overload (around line 126):
```cpp
  THROW_EVIDENCE_IF(bytesRead < 0, "tsk_fs_file_read() failed for inode " << Inum);
```

**Step 4: Run tests**

Run: `make -j8 check`

Expected: All tests pass.

**Step 5: Commit**

```bash
git add test/test_readseek.cpp src/readseek_impl.cpp
git commit -m "Make ReadSeekTSK throw EvidenceIOError on read failures

Evidence I/O errors are normal forensic conditions, not application
bugs. Using EvidenceIOError lets Processor catch and log them while
letting genuine application errors propagate."
```

---

### Task 4: TDD — Make ReadSeekFile throw EvidenceIOError

Same change for ReadSeekFile — use `THROW_EVIDENCE_IF` instead of `THROW_IF` for ferror.

**Files:**
- Modify: `src/readseek_impl.cpp`

**Step 1: Change both ReadSeekFile overloads**

In `src/readseek_impl.cpp`, change the two `THROW_IF(ret < len && std::ferror(...), ...)` calls in ReadSeekFile::read() to `THROW_EVIDENCE_IF(...)`:

Vector overload (around line 53):
```cpp
  THROW_EVIDENCE_IF(ret < len && std::ferror(FilePtr.get()), "call to fread() had error");
```

Raw pointer overload (around line 62):
```cpp
  THROW_EVIDENCE_IF(ret < len && std::ferror(FilePtr.get()), "call to fread() had error");
```

**Step 2: Run tests**

Run: `make -j8 check`

Expected: All tests pass.

**Step 3: Commit**

```bash
git add src/readseek_impl.cpp
git commit -m "Make ReadSeekFile throw EvidenceIOError on read failures

Consistent with ReadSeekTSK — all evidence I/O errors use the same
exception type."
```

---

### Task 5: TDD — Enrich Entry with evidence context

Entry currently has only `Addr` and a `ReadSeek` stream. Add the fields that Processor needs for exception logging.

**Files:**
- Modify: `include/entry.h`
- Modify: `test/test_readseek.cpp` (or a new test, if Entry is tested elsewhere)

**Step 1: Write a test for the enriched Entry**

Check if there's an existing Entry test file. If not, test inline. The key test: construct an Entry with full context, verify all fields are accessible.

Add to an appropriate test file (or create `test/test_entry.cpp` if none exists — check `Makefile.am` and `test/meson.build` for existing entry tests first):

```cpp
TEST_CASE("entryCarriesEvidenceContext") {
  std::vector<uint8_t> buf{1, 2, 3};
  auto rs = std::make_unique<ReadSeekBuf>(buf);
  Entry entry(42, std::move(rs));
  entry.EvidenceFile = "test.E01";
  entry.FsIndex = 1;
  entry.FsOffset = 32256;
  entry.AddrFlags = 0x05;  // alloc + used
  entry.Path = "/Users/test/file.txt";
  entry.FileSize = 1024;

  REQUIRE(entry.Addr == 42);
  REQUIRE(entry.EvidenceFile == "test.E01");
  REQUIRE(entry.FsIndex == 1);
  REQUIRE(entry.FsOffset == 32256);
  REQUIRE(entry.AddrFlags == 0x05);
  REQUIRE(entry.Path == "/Users/test/file.txt");
  REQUIRE(entry.FileSize == 1024);
}
```

**Step 2: Run test — RED (fields don't exist yet)**

Run: `make -j8 check`

Expected: Compile error — Entry doesn't have these fields.

**Step 3: Add fields to Entry**

Modify `include/entry.h`:

```cpp
#pragma once

#include "readseek.h"

class Entry {
public:
  Entry(uint64_t addr, std::unique_ptr<ReadSeek> rs) : Addr(addr), stream(std::move(rs)) {}
  ReadSeek& getStream() { return *stream; }

  uint64_t Addr;

  std::string EvidenceFile;
  uint32_t    FsIndex = 0;
  uint64_t    FsOffset = 0;
  uint32_t    AddrFlags = 0;
  std::string Path;
  uint64_t    FileSize = 0;

private:
  std::unique_ptr<ReadSeek> stream;
};
```

**Step 4: Run test — GREEN**

Run: `make -j8 check`

Expected: All tests pass.

**Step 5: Commit**

```bash
git add include/entry.h test/test_readseek.cpp
git commit -m "Enrich Entry with evidence context fields

Adds EvidenceFile, FsIndex, FsOffset, AddrFlags, Path, FileSize to
Entry. Populated by TskReader at construction so Processor has full
context for exception logging."
```

---

### Task 6: Populate Entry fields in TskReader

TskReader creates Entry objects in `addToBatch()`. Populate the new fields from the TSK data available at that call site. Also thread the `path` argument through `processFile` — it's currently ignored.

**Files:**
- Modify: `src/tskreader.cpp:87-118`
- Modify: `include/tskreader.h:53` (processFile signature comment)

**Step 1: No new test — the Entry test from Task 5 validates fields exist. Integration testing happens via the exception log.**

**Step 2: Thread path through processFile and populate Entry fields**

In `src/tskreader.cpp`, change `processFile`:

```cpp
TSK_RETVAL_ENUM TskReader::processFile(TSK_FS_FILE* fs_file, const char* path) {
  addToBatch(fs_file, path);
  return TSK_OK;
}
```

Change `addToBatch` signature to accept path:

```cpp
bool TskReader::addToBatch(TSK_FS_FILE* fs_file, const char* path) {
```

In the body, after creating the Entry (line 117), populate its fields:

```cpp
    auto entry = std::make_unique<Entry>(meta.addr, makeReadSeek(fs_file));
    entry->EvidenceFile = ImgPath;
    entry->FsIndex = FsIndex;
    entry->FsOffset = CurFsOffset;
    entry->AddrFlags = meta.flags;
    entry->Path = path ? path : "";
    entry->FileSize = meta.size;
    Input->push(std::move(entry));
```

Note: the current code does `Input->push(std::make_unique<Entry>(meta.addr, makeReadSeek(fs_file)))` — change to the above to set fields before pushing.

Update the header `include/tskreader.h` to match the new `addToBatch` signature:

```cpp
  bool addToBatch(TSK_FS_FILE* fs_file, const char* path);
```

And update `processFile` to remove the `/* path */` comment:

```cpp
  TSK_RETVAL_ENUM processFile(TSK_FS_FILE* fs_file, const char* path);
```

**Step 3: Run tests**

Run: `make -j8 check`

Expected: All tests pass.

**Step 4: Commit**

```bash
git add src/tskreader.cpp include/tskreader.h
git commit -m "Populate Entry evidence context from TskReader

Sets EvidenceFile, FsIndex, FsOffset, AddrFlags, Path, FileSize
on each Entry from the TSK metadata available at addToBatch time.
Threads the path argument through processFile instead of ignoring it."
```

---

### Task 7: TDD — Create ExceptionRecord struct and exception_log table

Define the DB record struct for the exception log and create the table at startup.

**Files:**
- Create: `include/duckexception.h`
- Modify: `src/llama.cpp:214-220`

**Step 1: Create the record struct**

Create `include/duckexception.h`:

```cpp
// ABOUTME: DuckDB record struct for the evidence exception log table
// ABOUTME: Stores context about evidence files that could not be read or processed

#pragma once

#include "llamaduck.h"

struct ExceptionRecord {
  static constexpr auto ColNames = {"evidence_file",
                                    "fs_index",
                                    "fs_offset",
                                    "addr",
                                    "addr_flags",
                                    "path",
                                    "file_size",
                                    "operation",
                                    "timestamp",
                                    "message"};

  std::string EvidenceFile;
  uint64_t    FsIndex;
  uint64_t    FsOffset;
  uint64_t    Addr;
  std::string AddrFlags;
  std::string Path;
  uint64_t    FileSize;
  std::string Operation;
  std::string Timestamp;
  std::string Message;
};

using ExceptionBatch = DBBatch<ExceptionRecord>;
```

Note: `FsIndex` is `uint64_t` here (not `uint32_t`) because the `duckdbType` template maps all integrals to UBIGINT. Using `uint32_t` in the struct would still work with pfr reflection but we keep types consistent with the DB column type. The nullable fields from the design (fs_index, fs_offset, addr, file_size) are stored as 0 when not applicable — DuckDB UBIGINT doesn't support NULL through the appender easily, and 0 is an unambiguous sentinel for these fields.

**Step 2: Add table creation to dbInit()**

In `src/llama.cpp`, add to `dbInit()`:

```cpp
#include "duckexception.h"

bool Llama::dbInit() {
  DBType<Dirent>::createTable(DbConn.get(), "dirent");
  DBType<Inode>::createTable(DbConn.get(), "inode");
  DBType<HashRec>::createTable(DbConn.get(), "hash");
  DBType<Extent>::createTable(DbConn.get(), "extents");
  DBType<ExceptionRecord>::createTable(DbConn.get(), "exception_log");
  return true;
}
```

**Step 3: Run tests**

Run: `make -j8 check`

Expected: All tests pass.

**Step 4: Commit**

```bash
git add include/duckexception.h src/llama.cpp
git commit -m "Add ExceptionRecord struct and exception_log table

Creates the exception_log DuckDB table at startup for recording
evidence I/O failures with full context."
```

---

### Task 8: TDD — Add exception counter to ProgressInfo

ProgressInfo needs an atomic counter for exceptions, and `formatLine()` should display it when nonzero.

**Files:**
- Modify: `test/test_progressinfo.cpp`
- Modify: `include/progressinfo.h`
- Modify: `src/progressinfo.cpp`

**Step 1: Write a failing test**

Look at `test/test_progressinfo.cpp` for the existing test pattern, then add:

```cpp
TEST_CASE("progressInfoExceptionCount") {
  ProgressInfo info;
  REQUIRE(info.exceptionCount() == 0);
  info.addException();
  REQUIRE(info.exceptionCount() == 1);
  info.addException();
  info.addException();
  REQUIRE(info.exceptionCount() == 3);
}

TEST_CASE("progressInfoFormatLineShowsExceptions") {
  ProgressInfo info;
  info.setFilesystem(1, 0, 100, 1024);
  info.update(10, 512);

  // No exceptions — should not contain "exception"
  std::string line = info.formatLine(1.0);
  REQUIRE(line.find("exception") == std::string::npos);

  // With exceptions — should contain count
  info.addException();
  info.addException();
  line = info.formatLine(1.0);
  REQUIRE(line.find("2 exceptions") != std::string::npos);
}
```

**Step 2: Run test — RED**

Run: `make -j8 check`

Expected: Compile error — `exceptionCount()` and `addException()` don't exist.

**Step 3: Implement**

In `include/progressinfo.h`, add to the public interface:

```cpp
  void addException();
  uint64_t exceptionCount() const;
```

Add to the private section:

```cpp
  std::atomic<uint64_t> ExceptionCount;
```

In `src/progressinfo.cpp`, add to the constructor's initializer list:

```cpp
  ExceptionCount(0)
```

Add the methods:

```cpp
void ProgressInfo::addException() {
  ExceptionCount.fetch_add(1);
}

uint64_t ProgressInfo::exceptionCount() const {
  return ExceptionCount.load();
}
```

In `formatLine()`, after the final `snprintf` that writes rates and elapsed time, add:

```cpp
  uint64_t exceptions = exceptionCount();
  if (exceptions > 0) {
    p += std::snprintf(p, end - p, " | %s exceptions",
                       formatWithCommas(exceptions).c_str());
  }
```

**Step 4: Run tests — GREEN**

Run: `make -j8 check`

Expected: All tests pass.

**Step 5: Commit**

```bash
git add test/test_progressinfo.cpp include/progressinfo.h src/progressinfo.cpp
git commit -m "Add exception counter to ProgressInfo with progress bar display

Atomic counter incremented by Processor on evidence I/O errors.
Displayed in progress bar as 'N exceptions' when nonzero."
```

---

### Task 9: TDD — Add exception logging to Processor

This is the core task. Add the exception batch appender to Processor, wrap each operation in `process()` with a try/catch for `EvidenceIOError`, and log exceptions.

**Files:**
- Modify: `include/processor.h`
- Modify: `src/processor.cpp`

**Step 1: Add the appender and batch members to Processor**

In `include/processor.h`, add `#include "duckexception.h"` and `#include "evidenceioerror.h"` at the top.

Add to the private section of `Processor`:

```cpp
  LlamaDBAppender   ExceptionAppender;
  std::unique_ptr<ExceptionBatch> Exceptions;
```

Add a private helper:

```cpp
  void logException(const Entry& entry, const char* operation, const char* message);
```

**Step 2: Initialize in constructor**

In `src/processor.cpp`, in the Processor constructor initializer list, add (after `FileSigAppender`):

```cpp
  ExceptionAppender(DbConn.get(), "exception_log"),
```

And in the body (or initializer list), add:

```cpp
  Exceptions(std::make_unique<ExceptionBatch>()),
```

**Step 3: Add flush for exceptions**

In `Processor::flush()`, add exception flushing. The exception batch should flush independently — don't gate it on `Hashes->size()` because we might have exceptions with no hashes (hash failure = early return):

```cpp
void Processor::flush(void) {
  if (Hashes->size()) {
    Hashes->copyToDB(HashAppender.get());
    SearchHits->copyToDB(SearchHitAppender.get());
    FileSigs->copyToDB(FileSigAppender.get());
    HashAppender.flush();
    SearchHitAppender.flush();
    FileSigAppender.flush();
    Hashes->clear();
    SearchHits->clear();
    FileSigs->clear();
  }
  if (Exceptions->size()) {
    Exceptions->copyToDB(ExceptionAppender.get());
    ExceptionAppender.flush();
    Exceptions->clear();
  }
}
```

**Step 4: Implement logException()**

In `src/processor.cpp`, add:

```cpp
#include "tskconversion.h"
#include <chrono>

void Processor::logException(const Entry& entry, const char* operation, const char* message) {
  auto now = std::chrono::system_clock::now();
  auto time_t_now = std::chrono::system_clock::to_time_t(now);
  char timebuf[32];
  std::strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%S", std::gmtime(&time_t_now));

  ExceptionRecord rec;
  rec.EvidenceFile = entry.EvidenceFile;
  rec.FsIndex = entry.FsIndex;
  rec.FsOffset = entry.FsOffset;
  rec.Addr = entry.Addr;
  rec.AddrFlags = TskUtils::metaFlags(entry.AddrFlags);
  rec.Path = entry.Path;
  rec.FileSize = entry.FileSize;
  rec.Operation = operation;
  rec.Timestamp = timebuf;
  rec.Message = message;

  Exceptions->add(rec);

  if (Context->Progress) {
    Context->Progress->addException();
  }
}
```

**Step 5: Wrap process() operations in try/catch**

Change `Processor::process()` in `src/processor.cpp`:

```cpp
void Processor::process(Entry& entry) {
  SFHASH_HashValues h;
  {
    Timer procTime;
    try {
      hashFile(Hasher.get(), entry.getStream(), Buf, h);
    } catch (const EvidenceIOError& e) {
      logException(entry, "hash", e.what());
      ProcTimeTotal += procTime.elapsed();
      return;
    }
    ProcTimeTotal += procTime.elapsed();
  }
  HashRecord.set(h, entry.Addr);

  if (Context->ExclusionHashset && Context->ExclusionHashset->lookup(h)) {
    // do something here if hash is in exclusion hset
  } else if (Context->InclusionHashset && Context->InclusionHashset->lookup(h)) {
    // add to rule hits here if hash in inclusion hset
    // since we want to treat inclusion hset as if it were another rule
  }

  // write hash record to database
  Hashes->add(HashRecord);

  // Detect file signatures
  bool hasPdfSig = false;
  {
    try {
      std::vector<MagicPtr> sigResults;
      entry.getStream().seek(0);
      SigAnalyzer.getSignatures(entry.getStream(), sigResults);
      for (const auto& sig : sigResults) {
        FileSigs->add(FileSigResult{HashRecord.Blake3, sig->Id});
        if (sig->Id == "8343f9e2-601f-4e88-8a78-09a3b5f906eb") {
          hasPdfSig = true;
        }
      }
    } catch (const EvidenceIOError& e) {
      logException(entry, "signature", e.what());
    }
  }

  {
    Timer procTime;
    try {
      if (hasPdfSig) {
        PDFReader reader;
        reader.readTextFromPDF(entry.getStream());
        char* text = reader.getExtractedText();
        if (text) {
          ReadSeekBuf rs(text);
          search(rs);
        }
        else {
          search(entry.getStream());
        }
      }
      else {
        search(entry.getStream());
      }
    } catch (const EvidenceIOError& e) {
      logException(entry, "search", e.what());
    }
    ProcTimeTotal += procTime.elapsed();
  }
}
```

**Step 6: Run tests**

Run: `make -j8 check`

Expected: All tests pass.

**Step 7: Commit**

```bash
git add include/processor.h src/processor.cpp
git commit -m "Add evidence exception handling to Processor

Wraps hash, signature, and search operations in try/catch for
EvidenceIOError. Logs exceptions to exception_log table with full
evidence context. Hash failure causes early return. Signature and
search failures are independent."
```

---

### Task 10: Add end-of-run exception summary

Print a message after processing if any exceptions occurred.

**Files:**
- Modify: `src/llama.cpp`

**Step 1: No new test — this is a stderr print for the examiner.**

**Step 2: Add summary after progress thread stops**

In `src/llama.cpp`, after `progressThread.stop()` (line 115), add:

```cpp
    if (progressInfo.exceptionCount() > 0) {
      std::cerr << progressInfo.exceptionCount()
                << " evidence I/O exceptions encountered -- see exception_log table\n";
    }
```

**Step 3: Run tests**

Run: `make -j8 check`

Expected: All tests pass.

**Step 4: Commit**

```bash
git add src/llama.cpp
git commit -m "Print evidence exception summary at end of processing

Reports count and directs examiner to exception_log table."
```

---

### Task 11: Integration test — run against donald_blake.E01

Rebuild and run against the evidence file that triggered the original crash.

**Step 1: Rebuild**

```bash
meson compile -C builddir
```

**Step 2: Run against evidence**

```bash
./builddir/src/llama dblake ~/ev/donald_blake.E01
```

Expected: No crash. Progress bar should show exception count. At the end, should print something like:

```
N evidence I/O exceptions encountered -- see exception_log table
```

**Step 3: Verify exception log content**

```bash
duckdb dblake/*.db "SELECT * FROM exception_log LIMIT 10"
```

Expected: Rows with inode 119652, flags showing "Deleted, Compressed", operation "hash", and the TSK error message about NTFS decompression.

**Step 4: No commit needed — this is a manual verification step.**

---

### Task 12: Final verification

**Step 1: Run the full test suite**

Run: `make -j8 check`

Expected: All tests pass.

**Step 2: Review the diff**

Run: `git log readseek-contract-hardening~10..HEAD --oneline`

Review that all changes are intentional.
