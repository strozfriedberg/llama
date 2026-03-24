# FileScheduler Bucketing Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Replace walk-order batch dispatch with disk-locality-aware bucketing so each Processor's reads are sequential within the libewf chunk cache, and large files are prioritized to avoid straggler tails.

**Architecture:** Entry gains a DiskOffset field populated by TskReader. BatchHandler sorts entries by disk offset and sends large files (>256MB) directly to FileScheduler. FileScheduler distributes entries into 256MB-span buckets and dispatches them via a priority queue ordered by total bytes. Processor sorts each batch by disk offset before processing. A new DuckDB `batches` table tracks dispatch metadata for post-run analysis.

**Tech Stack:** C++20, Catch2, DuckDB, Boost.Asio (strand, thread_pool)

---

### Task 1: Add DiskOffset field to Entry

Entry needs a DiskOffset field so bucketing can assign files to disk-region buckets.

**Files:**
- Modify: `include/entry.h:6-25`

**Step 1: Add the field**

In `include/entry.h`, add `DiskOffset` to the Entry class:

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
  uint32_t    AddrFlags = 0;
  std::string Path;
  uint64_t    FileSize = 0;
  uint64_t    DiskOffset = 0;
  TSK_FS_TYPE_ENUM FsType = TSK_FS_TYPE_DETECT;

private:
  std::unique_ptr<ReadSeek> stream;
};
```

**Step 2: Build and run tests**

Run: `make -j8 check`
Expected: All 362 tests pass. This is a data-only addition with no behavioral change.

**Step 3: Commit**

```
git add include/entry.h
git commit -m "Add DiskOffset field to Entry for bucketing"
```

---

### Task 2: Populate DiskOffset in TskReader

TskReader::addToBatch already has access to the TSK_FS_FILE. Extract the first non-resident data run's disk offset from the file's attributes.

**Files:**
- Modify: `src/tskreader.cpp:92-147` (addToBatch method)
- Test: `test/test_tskreader.cpp` (existing tests verify addToBatch still works)

**Step 1: Write a failing test**

In `test/test_tskreader.cpp`, add a test that verifies DiskOffset is populated for files with non-resident data runs. First, read the existing test file to understand its patterns and find a good place to add the test. The test should use the existing test disk image (`test/data/img.dd`) and verify that at least some entries have non-zero DiskOffset values.

Look at the existing TskReader tests to understand how they set up the reader and collect entries. The test should:
1. Open the test disk image
2. Walk it with a custom InputHandler that collects entries
3. Verify at least one entry has DiskOffset > 0

**Step 2: Run test to verify it fails**

Run: `make -j8 check`
Expected: New test FAILS because DiskOffset is never set (always 0).

**Step 3: Populate DiskOffset in addToBatch**

In `src/tskreader.cpp`, in the `addToBatch` method, after setting `entry->FileSize` (line 123) and before `Input->push(std::move(entry))` (line 124), extract the first non-resident data run offset:

```cpp
    entry->FileSize = meta.size;

    // Extract first non-resident data run offset for bucketing
    if (meta.attr) {
      for (const TSK_FS_ATTR* attr = meta.attr->head; attr; attr = attr->next) {
        if ((attr->type == TSK_FS_ATTR_TYPE_NTFS_DATA ||
             attr->type == TSK_FS_ATTR_TYPE_DEFAULT) &&
            (attr->flags & TSK_FS_ATTR_NONRES) && attr->nrd.run) {
          entry->DiskOffset = attr->nrd.run->addr * CurFsBlockSize;
          break;
        }
      }
    }

    Input->push(std::move(entry));
```

**Step 4: Run tests to verify pass**

Run: `make -j8 check`
Expected: All tests pass including the new DiskOffset test.

**Step 5: Commit**

```
git add src/tskreader.cpp test/test_tskreader.cpp
git commit -m "Populate Entry::DiskOffset from first non-resident data run"
```

---

### Task 3: Add BatchRec struct and batches table

Create the DuckDB record struct for the `batches` table and register it in `Llama::dbInit()`.

**Files:**
- Create: `include/duckbatch.h`
- Modify: `src/llama.cpp:220-227` (dbInit method)
- Test: `test/test_duckdb.cpp` (add table creation test)

**Step 1: Write a failing test**

In `test/test_duckdb.cpp`, add a test that creates the batches table and inserts/reads back a row. Look at the existing `testDuckHashTable` test for the pattern. The test should:
1. Create a DuckDB in memory
2. Create the batches table using `DBType<BatchRec>::createTable`
3. Insert a row via appender
4. Query it back and verify column names and values

This will fail because `BatchRec` doesn't exist yet.

**Step 2: Run test to verify it fails**

Run: `make -j8 check`
Expected: Compilation error — `BatchRec` not defined.

**Step 3: Create include/duckbatch.h**

```cpp
// ABOUTME: DuckDB record struct for the batch dispatch tracking table.
// ABOUTME: Stores metadata about each batch dispatched to a Processor for post-run analysis.

#pragma once

#include "llamaduck.h"

struct BatchRec {
  static constexpr auto ColNames = {"BatchId",
                                    "BucketIndex",
                                    "NumEntries",
                                    "TotalBytes"};

  uint64_t BatchId;
  uint64_t BucketIndex;
  uint64_t NumEntries;
  uint64_t TotalBytes;
};

using BatchBatch = DBBatch<BatchRec>;
```

Note: BucketIndex is uint64_t. Use sentinel values for special buckets: `UINT64_MAX` for resident bucket, `UINT64_MAX - 1` for large-file bypass. These will be defined as constants in FileScheduler.

**Step 4: Add include to the test file and verify it compiles**

Add `#include "duckbatch.h"` to the test and re-run.

**Step 5: Add table creation to dbInit**

In `src/llama.cpp`, add `#include "duckbatch.h"` to the includes, and in `dbInit()` after the existing table creations:

```cpp
bool Llama::dbInit() {
  DBType<Dirent>::createTable(DbConn.get(), "dirent");
  DBType<Inode>::createTable(DbConn.get(), "inode");
  DBType<HashRec>::createTable(DbConn.get(), "hash");
  DBType<Extent>::createTable(DbConn.get(), "extents");
  DBType<ExceptionRecord>::createTable(DbConn.get(), "exception_log");
  DBType<BatchRec>::createTable(DbConn.get(), "batches");
  return true;
}
```

**Step 6: Run tests to verify pass**

Run: `make -j8 check`
Expected: All tests pass.

**Step 7: Commit**

```
git add include/duckbatch.h src/llama.cpp test/test_duckdb.cpp
git commit -m "Add BatchRec struct and batches table for dispatch tracking"
```

---

### Task 4: Add Bucket struct and bucketing logic to FileScheduler

This is the core change. FileScheduler gains bucketing state: a vector of Bucket structs, a resident bucket, and a priority queue for dispatch.

**Files:**
- Modify: `include/filescheduler.h`
- Modify: `src/filescheduler.cpp`
- Create: `test/test_filescheduler.cpp`
- Modify: `Makefile.am` (add test file)
- Modify: `test/meson.build` (add test file)

**Step 1: Write failing tests for bucket distribution**

Create `test/test_filescheduler.cpp`. The tests need a minimal setup — no TSK, no real Processor. Use stub entries with fake DiskOffset and FileSize values. The tests should NOT need a real Processor pool or thread pool — test the bucketing logic in isolation.

First, study the existing FileScheduler interface. The bucketing methods will be new public methods on FileScheduler that can be tested without the full scheduling pipeline:

- `startFilesystem(uint64_t fsSize)` — initialize buckets
- `addToBucket(std::unique_ptr<Entry> entry)` — place entry in correct bucket
- `hasPendingBuckets()` — check if dispatch queue has work
- `popBatch()` — pop the highest-priority batch

Write these tests:

```cpp
#include <catch2/catch_test_macros.hpp>

#include "entry.h"
#include "filescheduler.h"

namespace {
  std::unique_ptr<Entry> makeEntry(uint64_t addr, uint64_t diskOffset, uint64_t fileSize) {
    auto e = std::make_unique<Entry>(addr);
    e->DiskOffset = diskOffset;
    e->FileSize = fileSize;
    return e;
  }
}

TEST_CASE("bucketDistributionByDiskOffset") {
  // 1GB filesystem = 4 buckets of 256MB each
  FileScheduler::BucketState buckets;
  buckets.startFilesystem(1024 * 1024 * 1024);

  // Entry at 100MB should go to bucket 0
  buckets.addToBucket(makeEntry(1, 100 * 1024 * 1024, 1000));
  // Entry at 300MB should go to bucket 1
  buckets.addToBucket(makeEntry(2, 300 * 1024 * 1024, 1000));
  // Entry at 700MB should go to bucket 2
  buckets.addToBucket(makeEntry(3, 700 * 1024 * 1024, 1000));

  REQUIRE(buckets.bucketEntryCount(0) == 1);
  REQUIRE(buckets.bucketEntryCount(1) == 1);
  REQUIRE(buckets.bucketEntryCount(2) == 1);
  REQUIRE(buckets.bucketEntryCount(3) == 0);
}

TEST_CASE("residentFilesGoToResidentBucket") {
  FileScheduler::BucketState buckets;
  buckets.startFilesystem(1024 * 1024 * 1024);

  // DiskOffset == 0 means resident
  buckets.addToBucket(makeEntry(1, 0, 500));
  buckets.addToBucket(makeEntry(2, 0, 500));

  REQUIRE(buckets.residentEntryCount() == 2);
}

TEST_CASE("bucketDispatchesWhenByteThresholdReached") {
  FileScheduler::BucketState buckets;
  buckets.startFilesystem(1024 * 1024 * 1024);

  REQUIRE(!buckets.hasPendingBatches());

  // Add entries to bucket 0 totaling > 256MB
  uint64_t bytesPerEntry = 128 * 1024 * 1024; // 128MB each
  buckets.addToBucket(makeEntry(1, 10, bytesPerEntry));
  REQUIRE(!buckets.hasPendingBatches());

  buckets.addToBucket(makeEntry(2, 20, bytesPerEntry));
  // 256MB total — should trigger
  REQUIRE(buckets.hasPendingBatches());
}

TEST_CASE("priorityQueueDispatchesLargestFirst") {
  FileScheduler::BucketState buckets;
  buckets.startFilesystem(2ULL * 1024 * 1024 * 1024); // 2GB

  // Fill bucket 0 with 256MB
  buckets.addToBucket(makeEntry(1, 10, 256 * 1024 * 1024));

  // Fill bucket 2 with 512MB (two entries)
  buckets.addToBucket(makeEntry(2, 512 * 1024 * 1024 + 10, 256 * 1024 * 1024));
  buckets.addToBucket(makeEntry(3, 512 * 1024 * 1024 + 20, 256 * 1024 * 1024));

  REQUIRE(buckets.hasPendingBatches());

  // Largest (512MB bucket 2) should come first
  auto batch1 = buckets.popBatch();
  REQUIRE(batch1.TotalBytes == 512 * 1024 * 1024);
  REQUIRE(batch1.Entries.size() == 2);

  auto batch2 = buckets.popBatch();
  REQUIRE(batch2.TotalBytes == 256 * 1024 * 1024);
  REQUIRE(batch2.Entries.size() == 1);
}

TEST_CASE("flushAllBucketsPushesPartialBuckets") {
  FileScheduler::BucketState buckets;
  buckets.startFilesystem(1024 * 1024 * 1024);

  // Add small entries that don't reach threshold
  buckets.addToBucket(makeEntry(1, 10, 1000));
  buckets.addToBucket(makeEntry(2, 300 * 1024 * 1024, 2000));
  buckets.addToBucket(makeEntry(3, 0, 500)); // resident

  REQUIRE(!buckets.hasPendingBatches());

  buckets.flushAllBuckets();

  // All three non-empty buckets should now be pending
  REQUIRE(buckets.hasPendingBatches());

  // Pop all three and verify
  size_t totalEntries = 0;
  while (buckets.hasPendingBatches()) {
    auto batch = buckets.popBatch();
    totalEntries += batch.Entries.size();
  }
  REQUIRE(totalEntries == 3);
}

TEST_CASE("startFilesystemFlushesExistingBuckets") {
  FileScheduler::BucketState buckets;
  buckets.startFilesystem(1024 * 1024 * 1024);

  buckets.addToBucket(makeEntry(1, 10, 1000));

  // Starting a new filesystem should flush the old bucket
  buckets.startFilesystem(512 * 1024 * 1024);

  REQUIRE(buckets.hasPendingBatches());
  auto batch = buckets.popBatch();
  REQUIRE(batch.Entries.size() == 1);
}

TEST_CASE("largeFileBecomesSingleEntryBatch") {
  FileScheduler::BucketState buckets;
  buckets.startFilesystem(1024 * 1024 * 1024);

  uint64_t largeSize = 300 * 1024 * 1024; // 300MB > threshold
  buckets.addLargeFile(makeEntry(1, 500 * 1024 * 1024, largeSize));

  REQUIRE(buckets.hasPendingBatches());
  auto batch = buckets.popBatch();
  REQUIRE(batch.Entries.size() == 1);
  REQUIRE(batch.TotalBytes == largeSize);
}
```

**Step 2: Run tests to verify they fail**

Run: `make -j8 check`
Expected: Compilation errors — `BucketState` doesn't exist.

**Step 3: Implement BucketState**

The bucketing logic is extracted into a nested `BucketState` class within FileScheduler. This keeps it testable without needing the full scheduler. Add the following to `include/filescheduler.h`:

```cpp
#pragma once

#include <memory>
#include <queue>
#include <vector>

#include <condition_variable>
#include <mutex>

#include "boost_asio.h"

#include "entry.h"
#include "llamaduck.h"
#include "duckbatch.h"
#include "direntbatch.h"
#include "duckinode.h"
#include "readseek.h"

struct FileRecord;
struct Options;
class OutputHandler;
class Processor;

class FileScheduler {
public:
  static constexpr uint64_t BUCKET_SPAN = 256ULL * 1024 * 1024;
  static constexpr uint64_t BUCKET_BYTE_LIMIT = BUCKET_SPAN;
  static constexpr uint64_t LARGE_FILE_THRESHOLD = BUCKET_SPAN;
  static constexpr uint64_t RESIDENT_BUCKET_INDEX = UINT64_MAX;
  static constexpr uint64_t LARGE_FILE_BUCKET_INDEX = UINT64_MAX - 1;

  struct DispatchBatch {
    std::vector<std::unique_ptr<Entry>> Entries;
    uint64_t TotalBytes = 0;
    uint64_t BucketIndex = 0;
  };

  class BucketState {
  public:
    void startFilesystem(uint64_t fsSize);
    void addToBucket(std::unique_ptr<Entry> entry);
    void addLargeFile(std::unique_ptr<Entry> entry);
    void flushAllBuckets();

    bool hasPendingBatches() const;
    DispatchBatch popBatch();

    size_t bucketEntryCount(size_t idx) const;
    size_t residentEntryCount() const;

  private:
    struct Bucket {
      std::vector<std::unique_ptr<Entry>> Entries;
      uint64_t TotalBytes = 0;
      uint64_t BucketIndex = 0;

      bool ready() const { return TotalBytes >= BUCKET_BYTE_LIMIT; }
      bool empty() const { return Entries.empty(); }
    };

    struct PriorityCompare {
      bool operator()(const DispatchBatch& a, const DispatchBatch& b) const {
        return a.TotalBytes < b.TotalBytes;
      }
    };

    std::vector<Bucket> Buckets;
    Bucket ResidentBucket;
    std::priority_queue<DispatchBatch, std::vector<DispatchBatch>, PriorityCompare> DispatchQueue;

    void maybePushBucket(Bucket& bucket);
    DispatchBatch makeBatch(Bucket& bucket);
  };

  FileScheduler(LlamaDB& db, boost::asio::thread_pool& pool,
                const std::shared_ptr<Processor>& protoProc,
                const std::shared_ptr<Options>& opts);

  void scheduleFileBatch(const DirentBatch& dirents,
                         const InodeBatch& inodes,
                         const std::shared_ptr<std::vector<std::unique_ptr<Entry>>>& entries);

  void startFilesystem(uint64_t fsSize);
  void flushAllBuckets();

  double getProcessorTime();

private:
  void performScheduling(DirentBatch& dirents,
                         InodeBatch& inodes,
                         const std::shared_ptr<std::vector<std::unique_ptr<Entry>>>& entries);

  void writeDirentsAndInodes(DirentBatch& dirents, InodeBatch& inodes);
  void dispatchIfReady();

  std::shared_ptr<Processor> popProc();
  void pushProc(const std::shared_ptr<Processor>& proc);

  LlamaDBConnection DBConn;

  boost::asio::thread_pool& Pool;
  boost::asio::strand<boost::asio::thread_pool::executor_type> Strand;

  std::vector<std::shared_ptr<Processor>> Processors;

  std::mutex ProcMutex;
  std::condition_variable ProcCV;

  BucketState Buckets;

  uint64_t NextBatchId = 0;
  BatchBatch BatchLog;
  LlamaDBAppender BatchAppender;
};
```

**Step 4: Implement BucketState in filescheduler.cpp**

Add the BucketState method implementations to `src/filescheduler.cpp`:

```cpp
void FileScheduler::BucketState::startFilesystem(uint64_t fsSize) {
  flushAllBuckets();
  size_t numBuckets = (fsSize + BUCKET_SPAN - 1) / BUCKET_SPAN;
  Buckets.clear();
  Buckets.resize(numBuckets);
  for (size_t i = 0; i < numBuckets; ++i) {
    Buckets[i].BucketIndex = i;
  }
  ResidentBucket = Bucket{};
  ResidentBucket.BucketIndex = RESIDENT_BUCKET_INDEX;
}

void FileScheduler::BucketState::addToBucket(std::unique_ptr<Entry> entry) {
  if (entry->DiskOffset == 0) {
    ResidentBucket.TotalBytes += entry->FileSize;
    ResidentBucket.Entries.push_back(std::move(entry));
    maybePushBucket(ResidentBucket);
    return;
  }

  size_t idx = entry->DiskOffset / BUCKET_SPAN;
  if (idx >= Buckets.size()) {
    idx = Buckets.size() - 1;
  }
  auto& bucket = Buckets[idx];
  bucket.TotalBytes += entry->FileSize;
  bucket.Entries.push_back(std::move(entry));
  maybePushBucket(bucket);
}

void FileScheduler::BucketState::addLargeFile(std::unique_ptr<Entry> entry) {
  DispatchBatch batch;
  batch.TotalBytes = entry->FileSize;
  batch.BucketIndex = LARGE_FILE_BUCKET_INDEX;
  batch.Entries.push_back(std::move(entry));
  DispatchQueue.push(std::move(batch));
}

void FileScheduler::BucketState::flushAllBuckets() {
  for (auto& bucket : Buckets) {
    if (!bucket.empty()) {
      DispatchQueue.push(makeBatch(bucket));
    }
  }
  Buckets.clear();
  if (!ResidentBucket.empty()) {
    DispatchQueue.push(makeBatch(ResidentBucket));
  }
}

bool FileScheduler::BucketState::hasPendingBatches() const {
  return !DispatchQueue.empty();
}

FileScheduler::DispatchBatch FileScheduler::BucketState::popBatch() {
  auto batch = std::move(const_cast<DispatchBatch&>(DispatchQueue.top()));
  DispatchQueue.pop();
  return batch;
}

size_t FileScheduler::BucketState::bucketEntryCount(size_t idx) const {
  if (idx >= Buckets.size()) return 0;
  return Buckets[idx].Entries.size();
}

size_t FileScheduler::BucketState::residentEntryCount() const {
  return ResidentBucket.Entries.size();
}

void FileScheduler::BucketState::maybePushBucket(Bucket& bucket) {
  if (bucket.ready()) {
    DispatchQueue.push(makeBatch(bucket));
  }
}

FileScheduler::DispatchBatch FileScheduler::BucketState::makeBatch(Bucket& bucket) {
  DispatchBatch batch;
  batch.TotalBytes = bucket.TotalBytes;
  batch.BucketIndex = bucket.BucketIndex;
  batch.Entries = std::move(bucket.Entries);
  bucket.Entries.clear();
  bucket.TotalBytes = 0;
  return batch;
}
```

**Step 5: Add test file to build systems**

In `Makefile.am`, add `test/test_filescheduler.cpp` to `test_test_SOURCES` (after `test/test_filesignatures.cpp`).

In `test/meson.build`, add `'test_filescheduler.cpp'` to the `test_sources` list.

**Step 6: Run tests to verify pass**

Run: `make -j8 check`
Expected: All tests pass including the new bucketing tests.

**Step 7: Commit**

```
git add include/filescheduler.h src/filescheduler.cpp test/test_filescheduler.cpp Makefile.am test/meson.build
git commit -m "Add BucketState with disk-locality bucketing and priority dispatch"
```

---

### Task 5: Wire bucketing into FileScheduler dispatch

Replace the current direct-dispatch in `performScheduling` with the bucketing flow. FileScheduler now writes dirents/inodes to DB, distributes entries into buckets, and dispatches when a Processor becomes available.

**Files:**
- Modify: `src/filescheduler.cpp`
- Modify: `include/filescheduler.h` (if needed for BatchAppender initialization)

**Step 1: Update FileScheduler constructor**

The constructor needs to initialize the BatchAppender. Update the constructor:

```cpp
FileScheduler::FileScheduler(LlamaDB& db,
                             boost::asio::thread_pool& pool,
                             const std::shared_ptr<Processor>& protoProc,
                             const std::shared_ptr<Options>& opts)
    : DBConn(db), Pool(pool), Strand(Pool.get_executor()),
      ProcMutex(), ProcCV(),
      NextBatchId(0), BatchAppender(DBConn.get(), "batches") {
  for (unsigned int i = 0; i < opts->NumThreads; ++i) {
    Processors.push_back(protoProc->clone());
  }
}
```

**Step 2: Implement writeDirentsAndInodes**

Extract the DB write logic from `performScheduling` into its own method:

```cpp
void FileScheduler::writeDirentsAndInodes(DirentBatch& dirents, InodeBatch& inodes) {
  std::string tmpDents = "_temp_dirent";
  std::string tmpInodes = "_temp_inode";

  DBType<Dirent>::createTable(DBConn.get(), tmpDents);
  DBType<Inode>::createTable(DBConn.get(), tmpInodes);

  LlamaDBAppender dAppender(DBConn.get(), tmpDents);
  LlamaDBAppender iAppender(DBConn.get(), tmpInodes);

  dirents.copyToDB(dAppender.get());
  dAppender.flush();
  inodes.copyToDB(iAppender.get());
  iAppender.flush();

  duckdb_result result;
  auto state = duckdb_query(DBConn.get(), "INSERT INTO dirent SELECT * FROM _temp_dirent;", &result);
  THROW_IF(state == DuckDBError, "Error inserting into dirent table");
  state = duckdb_query(DBConn.get(), "INSERT INTO inode SELECT * FROM _temp_inode;", &result);
  THROW_IF(state == DuckDBError, "Error inserting into inode table");

  state = duckdb_query(DBConn.get(), "DROP TABLE _temp_dirent;", &result);
  THROW_IF(state == DuckDBError, "Error dropping _temp_dirent table");
  state = duckdb_query(DBConn.get(), "DROP TABLE _temp_inode;", &result);
  THROW_IF(state == DuckDBError, "Error dropping _temp_inode table");
}
```

**Step 3: Implement dispatchIfReady**

```cpp
void FileScheduler::dispatchIfReady() {
  while (Buckets.hasPendingBatches()) {
    // Try to get a Processor without blocking
    std::unique_lock<std::mutex> lock(ProcMutex);
    if (Processors.empty()) {
      return; // No Processors available; we'll try again when one returns
    }
    auto proc = Processors.back();
    Processors.pop_back();
    lock.unlock();

    auto batch = Buckets.popBatch();

    // Log the batch
    BatchLog.add(BatchRec{NextBatchId++, batch.BucketIndex,
                          batch.Entries.size(), batch.TotalBytes});
    BatchLog.copyToDB(BatchAppender.get());
    BatchAppender.flush();
    BatchLog.clear();

    auto entries = std::make_shared<std::vector<std::unique_ptr<Entry>>>(
      std::move(batch.Entries));
    boost::asio::post(Pool, [=, this]() {
      proc->processBatch(entries);
      this->pushProc(proc);
    });
  }
}
```

**Step 4: Update performScheduling to use bucketing**

```cpp
void FileScheduler::performScheduling(DirentBatch& dirents,
                                      InodeBatch& inodes,
                                      const std::shared_ptr<std::vector<std::unique_ptr<Entry>>>& entries)
{
  writeDirentsAndInodes(dirents, inodes);

  for (auto& entry : *entries) {
    Buckets.addToBucket(std::move(entry));
  }

  dispatchIfReady();
}
```

**Step 5: Update pushProc to trigger dispatch**

When a Processor returns, check if there are pending batches:

```cpp
void FileScheduler::pushProc(const std::shared_ptr<Processor>& proc) {
  {
    std::unique_lock<std::mutex> lock(ProcMutex);
    Processors.push_back(proc);
    ProcCV.notify_one();
  }
  boost::asio::post(Strand, [this]() { dispatchIfReady(); });
}
```

**Step 6: Implement startFilesystem and flushAllBuckets forwarding**

```cpp
void FileScheduler::startFilesystem(uint64_t fsSize) {
  boost::asio::post(Strand, [this, fsSize]() {
    Buckets.startFilesystem(fsSize);
    dispatchIfReady();
  });
}

void FileScheduler::flushAllBuckets() {
  boost::asio::post(Strand, [this]() {
    Buckets.flushAllBuckets();
    dispatchIfReady();
  });
}
```

**Step 7: Run tests to verify pass**

Run: `make -j8 check`
Expected: All tests pass. The existing `scheduleFileBatch` callers still work because the interface hasn't changed — only the internal dispatch strategy.

**Step 8: Commit**

```
git add include/filescheduler.h src/filescheduler.cpp
git commit -m "Wire bucketing into FileScheduler dispatch pipeline"
```

---

### Task 6: Add large-file handling and sort-by-offset to BatchHandler

BatchHandler needs to detect large files (>256MB) and send them to FileScheduler immediately. Normal batches should be sorted by DiskOffset before sending.

**Files:**
- Modify: `include/batchhandler.h`
- Modify: `src/batchhandler.cpp`
- Create or modify: `test/test_batchhandler.cpp` (new test file)
- Modify: `Makefile.am` (add test file)
- Modify: `test/meson.build` (add test file)

**Step 1: Write failing tests**

Create `test/test_batchhandler.cpp`. Test two behaviors:
1. Entries are sorted by DiskOffset before flush
2. Large files trigger an immediate flush

These tests need a mock/stub FileScheduler or a way to capture what BatchHandler sends. The simplest approach: create a test spy that implements the same interface or captures the entries BatchHandler sends to FileScheduler.

Since BatchHandler takes a `std::shared_ptr<FileScheduler>` and FileScheduler is a concrete class, the test needs to either:
- Use a real FileScheduler (heavy, but possible with in-memory DuckDB)
- Or add a thin interface

Given YAGNI and the existing codebase pattern, use a real FileScheduler with an in-memory DuckDB. The test doesn't need to verify dispatch — just that entries arrive at FileScheduler in the right order and at the right time.

Actually, the simpler approach: test the sort behavior by verifying that `CurEntries` is sorted after a flush is triggered. But `CurEntries` is private. Instead, add a `scheduleLargeFile` method to FileScheduler that BatchHandler calls for big files, and test that BatchHandler calls it.

The cleanest test: verify the sort by hooking into FileScheduler's `scheduleFileBatch` and checking entry order. This requires a real (but minimal) FileScheduler setup. Study `test/test_processor.cpp` for how to set up in-memory DuckDB with the required tables.

Write tests that:
1. Push several entries with various DiskOffsets, call `flush()`, verify the entries sent to FileScheduler are sorted by DiskOffset
2. Push a >256MB entry, verify it triggers an immediate flush before the normal batch threshold

**Step 2: Run tests to verify they fail**

**Step 3: Add scheduleLargeFile to FileScheduler**

In `include/filescheduler.h`, add:

```cpp
void scheduleLargeFile(const DirentBatch& dirents,
                       const InodeBatch& inodes,
                       std::unique_ptr<Entry> entry);
```

In `src/filescheduler.cpp`:

```cpp
void FileScheduler::scheduleLargeFile(const DirentBatch& dirents,
                                      const InodeBatch& inodes,
                                      std::unique_ptr<Entry> entry)
{
  auto dPtr = std::make_shared<DirentBatch>(dirents);
  auto iPtr = std::make_shared<InodeBatch>(inodes);
  auto ePtr = std::make_shared<std::unique_ptr<Entry>>(std::move(entry));
  boost::asio::post(
    Strand,
    [=, this]() mutable {
      writeDirentsAndInodes(*dPtr, *iPtr);
      Buckets.addLargeFile(std::move(*ePtr));
      dispatchIfReady();
    }
  );
}
```

**Step 4: Modify BatchHandler**

In `src/batchhandler.cpp`:

```cpp
#include "batchhandler.h"

#include "filescheduler.h"
#include "duckinode.h"

#include <algorithm>

namespace {
  const unsigned int BATCH_SIZE = 50;
  const uint64_t LARGE_FILE_THRESHOLD = 256ULL * 1024 * 1024;
}

BatchHandler::BatchHandler(std::shared_ptr<FileScheduler> sink):
  Sink(sink),
  CurDents(new DirentBatch()),
  CurInodes(new InodeBatch()),
  CurEntries(new std::vector<std::unique_ptr<Entry>>())
{
}

void BatchHandler::push(const Dirent& d) {
  CurDents->add(d);
}

void BatchHandler::push(const Inode& i) {
  CurInodes->add(i);
}

void BatchHandler::push(std::unique_ptr<Entry> entry) {
  if (entry->FileSize > LARGE_FILE_THRESHOLD) {
    Sink->scheduleLargeFile(*CurDents, *CurInodes, std::move(entry));
    CurDents->clear();
    CurInodes->clear();
    return;
  }
  CurEntries->push_back(std::move(entry));
}

void BatchHandler::maybeFlush() {
  if (CurInodes->size() > BATCH_SIZE) {
    flush();
  }
}

void BatchHandler::flush() {
  // Sort entries by disk offset for sequential bucket access
  std::sort(CurEntries->begin(), CurEntries->end(),
    [](const std::unique_ptr<Entry>& a, const std::unique_ptr<Entry>& b) {
      return a->DiskOffset < b->DiskOffset;
    });
  Sink->scheduleFileBatch(*CurDents, *CurInodes, CurEntries);
  CurDents->clear();
  CurInodes->clear();
  CurEntries.reset(new std::vector<std::unique_ptr<Entry>>());
}
```

**Step 5: Add test file to build systems**

In `Makefile.am`, add `test/test_batchhandler.cpp` to `test_test_SOURCES`.

In `test/meson.build`, add `'test_batchhandler.cpp'` to the `test_sources` list.

**Step 6: Run tests to verify pass**

Run: `make -j8 check`
Expected: All tests pass.

**Step 7: Commit**

```
git add include/batchhandler.h src/batchhandler.cpp include/filescheduler.h src/filescheduler.cpp test/test_batchhandler.cpp Makefile.am test/meson.build
git commit -m "BatchHandler sorts by disk offset and sends large files immediately"
```

---

### Task 7: Call startFilesystem from TskReader and flushAllBuckets at end of walk

TskReader needs to tell FileScheduler when a new filesystem starts and when the walk is complete.

**Files:**
- Modify: `include/inputhandler.h` (add startFilesystem/flushAll to interface)
- Modify: `include/batchhandler.h` (implement new methods)
- Modify: `src/batchhandler.cpp` (implement forwarding)
- Modify: `src/tskreader.cpp` (call the new methods)

**Step 1: Add methods to InputHandler interface**

In `include/inputhandler.h`:

```cpp
class InputHandler {
public:
  virtual ~InputHandler() {}

  virtual void push(const Dirent&) = 0;
  virtual void push(const Inode&) = 0;
  virtual void push(std::unique_ptr<Entry>) = 0;

  virtual void maybeFlush() = 0;
  virtual void flush() = 0;

  virtual void startFilesystem(uint64_t fsSize) = 0;
  virtual void flushAllBuckets() = 0;
};
```

**Step 2: Implement in BatchHandler**

In `include/batchhandler.h`, add:

```cpp
  virtual void startFilesystem(uint64_t fsSize) override;
  virtual void flushAllBuckets() override;
```

In `src/batchhandler.cpp`:

```cpp
void BatchHandler::startFilesystem(uint64_t fsSize) {
  flush(); // flush any pending entries from previous filesystem
  Sink->startFilesystem(fsSize);
}

void BatchHandler::flushAllBuckets() {
  flush(); // flush any pending entries
  Sink->flushAllBuckets();
}
```

**Step 3: Call from TskReader::filterFs**

In `src/tskreader.cpp`, in `filterFs()`, add a call to `startFilesystem` at the end (after setting CurFsBlockSize):

```cpp
TSK_FILTER_ENUM TskReader::filterFs(TSK_FS_INFO* fs_info) {
  Asm.addFileSystem(Tsk->convertFS(*fs_info));
  Tsg = Tsk->makeTimestampGetter(fs_info->ftype);
  CurFsOffset = fs_info->offset;
  CurFsBlockSize = fs_info->block_size;
  InodeTracker.clear();
  InodeTracker.resize(fs_info->last_inum - fs_info->first_inum + 1, false);
  if (Progress) {
    Progress->setFilesystem(++FsIndex, 0, fs_info->inum_count,
                            static_cast<uint64_t>(fs_info->block_count) * fs_info->block_size);
  }
  Input->startFilesystem(static_cast<uint64_t>(fs_info->block_count) * fs_info->block_size);
  return TSK_FILTER_CONT;
}
```

**Step 4: Call flushAllBuckets at end of walk**

In `src/tskreader.cpp`, in `startReading()`, after the final `Input->flush()`:

```cpp
  if (ret) {
    while (!Dirents.empty()) {
      Input->push(Dirents.pop());
    }
    Input->flush();
    Input->flushAllBuckets();
  }
```

**Step 5: Run tests to verify pass**

Run: `make -j8 check`
Expected: All tests pass. Any existing InputHandler implementations in tests may need the new virtual methods added (as no-ops if they don't care about bucketing).

Check for other InputHandler implementations that need updating:

```bash
grep -r "class.*InputHandler" include/ src/ test/
```

If there are test stubs or other implementations, add empty implementations of the new methods.

**Step 6: Commit**

```
git add include/inputhandler.h include/batchhandler.h src/batchhandler.cpp src/tskreader.cpp
git commit -m "Wire filesystem lifecycle events from TskReader through to FileScheduler"
```

---

### Task 8: Processor sorts batch by disk offset

Processor should sort the entries by DiskOffset before processing to maximize sequential reads within the libewf cache.

**Files:**
- Modify: `src/processor.cpp:191-216` (processBatch method)
- Test: `test/test_processor.cpp` (add sort verification test)

**Step 1: Write a failing test**

In `test/test_processor.cpp`, add a test that verifies processBatch processes entries in DiskOffset order. The simplest way: create entries with known DiskOffsets in reverse order, process them, and verify (via search hit order or hash record order) that they were processed in sorted order.

Actually, the most direct test: verify the sort happens by checking that after processBatch, entries were visited in DiskOffset order. Since Processor writes hash records to the database in processing order, insert entries with distinct addresses and check the DB order matches DiskOffset order.

This test requires some setup. Study the existing `test_processor.cpp` patterns for how to create a Processor with an in-memory DuckDB. The test should:
1. Create entries with DiskOffsets in descending order (e.g., 300MB, 200MB, 100MB)
2. Call processBatch
3. Verify hash records appear in ascending DiskOffset order

**Step 2: Run test to verify it fails**

Expected: FAIL — entries processed in original (descending) order.

**Step 3: Add sort to processBatch**

In `src/processor.cpp`, at the start of `processBatch`, add:

```cpp
void Processor::processBatch(const std::shared_ptr<std::vector<std::unique_ptr<Entry>>>& entries) {
  // Sort by disk offset for sequential I/O within libewf cache
  std::sort(entries->begin(), entries->end(),
    [](const std::unique_ptr<Entry>& a, const std::unique_ptr<Entry>& b) {
      return a->DiskOffset < b->DiskOffset;
    });

  uint64_t pendingInodes = 0;
  // ... rest unchanged
```

Add `#include <algorithm>` to `src/processor.cpp` if not already present.

**Step 4: Run tests to verify pass**

Run: `make -j8 check`
Expected: All tests pass.

**Step 5: Commit**

```
git add src/processor.cpp test/test_processor.cpp
git commit -m "Processor sorts batch by DiskOffset for sequential I/O"
```

---

### Task 9: End-to-end verification

Run the full test suite and verify everything works together. If a test disk image is available, run llama against it and verify the `batches` table is populated.

**Step 1: Run full test suite**

Run: `make -j8 check`
Expected: All tests pass.

**Step 2: Manual verification (if test image available)**

If the nfury image or test image is accessible, run llama and check:

```sql
SELECT * FROM batches ORDER BY BatchId;
SELECT BucketIndex, COUNT(*) as NumBatches, SUM(NumEntries) as TotalFiles, SUM(TotalBytes) as TotalBytes
FROM batches GROUP BY BucketIndex ORDER BY TotalBytes DESC;
```

Verify:
- Batches exist in the table
- Large files (BucketIndex = `UINT64_MAX - 1`) appear
- Multiple bucket indices are represented
- TotalBytes values are reasonable

**Step 3: Commit any fixes**

If any issues are found, fix and commit.
