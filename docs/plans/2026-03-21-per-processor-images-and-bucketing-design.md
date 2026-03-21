# Per-Processor TSK Images and FileScheduler Bucketing

## Problem

Llama's current architecture shares a single TSK image handle across all
processing threads. TSK uses heavy internal locking, so concurrent data reads
serialize on those mutexes. On a 52GB E01 image, we see ~186 MB/s throughput
with only ~2 cores utilized despite having a full thread pool — indicating lock
contention, not CPU saturation.

Additionally, FileScheduler dispatches batches in walk order without regard for
disk locality. This matters especially for future S3-backed reads (via a Rust
E01 library), where non-sequential access incurs HTTP range request latency.

## Implementation Order

1. **Per-Processor image refactor** — eliminate TSK lock contention
2. **FileScheduler bucketing** — optimize I/O locality

The bucketing provides little benefit until the lock contention is resolved,
since threads are currently blocked on TSK mutexes rather than on I/O scheduling.

## Part 1: Per-Processor TSK Images

### Entry Refactor

Entry becomes an abstract base class. It no longer owns a ReadSeek at creation
time. Instead, it carries enough information for a Processor to create the
ReadSeek on demand.

```cpp
class Entry {
public:
    Entry(uint64_t addr) : Addr(addr) {}
    virtual ~Entry() = default;

    virtual std::unique_ptr<ReadSeek> createReadSeek(ReadSeekContext& ctx) = 0;
    virtual uint64_t fileSize() const = 0;

    uint64_t Addr;
    uint64_t DiskOffset = 0;  // first data run offset, for bucketing
};
```

TskEntry is the first concrete subclass:

```cpp
class TskEntry : public Entry {
public:
    TskEntry(uint64_t addr, TSK_OFF_T fsOffset,
             TSK_FS_TYPE_ENUM fsType, uint64_t size,
             uint64_t diskOffset)
        : Entry(addr), FsOffset(fsOffset),
          FsType(fsType), Size(size)
    {
        DiskOffset = diskOffset;
    }

    std::unique_ptr<ReadSeek> createReadSeek(ReadSeekContext& ctx) override {
        auto fs = ctx.getFilesystemHandle(FsOffset, FsType);
        return std::make_unique<ReadSeekTSK>(fs, Addr);
    }

    uint64_t fileSize() const override { return Size; }

private:
    TSK_OFF_T FsOffset;
    TSK_FS_TYPE_ENUM FsType;
    uint64_t Size;
};
```

### ReadSeekContext Interface

A narrow interface that Processor implements, allowing Entry subclasses to
obtain the handles they need:

```cpp
class ReadSeekContext {
public:
    virtual ~ReadSeekContext() = default;
    virtual std::shared_ptr<TSK_FS_INFO> getFilesystemHandle(
        TSK_OFF_T offset, TSK_FS_TYPE_ENUM type) = 0;
};
```

This interface will grow when archive/PDF entry types arrive and the
Volume/Filesystem abstraction takes shape.

### Processor Changes

Each Processor opens its own TSK image and filesystem handles. The image path
flows through ProcessorContext (which already holds shared config).

```cpp
struct ProcessorContext {
    // ...existing members...
    std::string ImgPath;
};
```

Processor gains per-instance image state:

```cpp
class Processor : public ReadSeekContext {
    // ...existing members...

    std::string ImgPath;
    std::unique_ptr<TSK_IMG_INFO, void(*)(TSK_IMG_INFO*)> Img;
    std::unordered_map<TSK_OFF_T, std::shared_ptr<TSK_FS_INFO>> FsHandles;
};
```

Image and filesystem handles are opened lazily on first use:

```cpp
std::shared_ptr<TSK_FS_INFO> Processor::getFilesystemHandle(
    TSK_OFF_T offset, TSK_FS_TYPE_ENUM type)
{
    if (!Img) {
        Img = tsk_img_open(/* Context->ImgPath */);
        // throws std::runtime_error on failure
    }
    auto [itr, absent] = FsHandles.try_emplace(offset, nullptr);
    if (absent) {
        itr->second.reset(
            tsk_fs_open_img(Img.get(), offset, type),
            tsk_fs_close);
    }
    return itr->second;
}
```

`Processor::clone()` creates a fresh instance — the new Processor's image opens
lazily when it receives its first batch.

`Processor::processBatch` changes to call `entry->createReadSeek(*this)` instead
of `entry->getStream()`.

### TskReader Changes

`TskReader::addToBatch` creates TskEntry instead of Entry, extracting the first
data run offset from the TSK_FS_FILE attributes:

```cpp
uint64_t diskOffset = 0;
if (fs_file->meta->attr) {
    for (auto* attr = fs_file->meta->attr->head; attr; attr = attr->next) {
        if (attr->type == TSK_FS_ATTR_TYPE_NTFS_DATA ||
            attr->type == TSK_FS_ATTR_TYPE_DEFAULT) {
            if (attr->flags & TSK_FS_ATTR_NONRES && attr->nrd.run) {
                diskOffset = attr->nrd.run->addr * CurFsBlockSize;
                break;
            }
        }
    }
}
```

Out-of-range data run offsets (beyond filesystem size) are validated here and
logged. Such files are treated as resident.

`TskReader::makeReadSeek()` is no longer called — Entry no longer owns a
ReadSeek at walk time.

## Part 2: FileScheduler Bucketing

### Bucket Structure

```cpp
struct Bucket {
    std::vector<std::unique_ptr<Entry>> Entries;
    uint64_t TotalBytes = 0;
    steady_clock::time_point FirstInsert;

    bool empty() const { return Entries.empty(); }
};
```

### FileScheduler Changes

```cpp
class FileScheduler {
    // Bucketing
    std::vector<Bucket> Buckets;
    uint64_t BucketSpan;

    // Dedicated bucket for resident files (sorted by inode)
    Bucket ResidentBucket;

    // Dispatch tracking
    std::deque<size_t> ReadyQueue;
    std::unordered_set<size_t> ActiveBuckets;

    // Thresholds
    size_t BatchFileLimit = 256;
    steady_clock::duration MaxAge = seconds(2);

    // Processor availability
    std::atomic<unsigned> AvailableProcs;

    // ...existing DB/strand/pool members...
};
```

### Per-Filesystem Lifecycle

Bucketing operates per-filesystem. When a new filesystem starts, all previous
buckets are flushed:

```cpp
void FileScheduler::startFilesystem(uint64_t fsSize) {
    flushAllBuckets();
    BucketSpan = 64 * 1024 * 1024;  // 64MB
    size_t numBuckets = (fsSize + BucketSpan - 1) / BucketSpan;
    Buckets.clear();
    Buckets.resize(numBuckets);
    ActiveBuckets.clear();
    ReadyQueue.clear();
}
```

Called from the walk when `filterFs` encounters a new filesystem. An explicit
`flushAllBuckets()` is called after the walk completes to drain the last
filesystem's entries.

### Entry Distribution

O(1) per entry. Resident files (DiskOffset == 0) go to the dedicated resident
bucket.

```cpp
void addToBucket(std::unique_ptr<Entry> entry) {
    if (entry->DiskOffset == 0) {
        auto& bucket = ResidentBucket;
        if (bucket.empty()) {
            bucket.FirstInsert = steady_clock::now();
        }
        bucket.TotalBytes += entry->fileSize();
        bucket.Entries.push_back(std::move(entry));
        if (bucket.Entries.size() >= BatchFileLimit) {
            // resident bucket is ready; will be sorted by inode at dispatch
            ReadyQueue.push_back(RESIDENT_BUCKET_SENTINEL);
        }
        return;
    }

    size_t idx = entry->DiskOffset / BucketSpan;
    auto& bucket = Buckets[idx];
    if (bucket.empty()) {
        bucket.FirstInsert = steady_clock::now();
        ActiveBuckets.insert(idx);
    }
    bucket.TotalBytes += entry->fileSize();
    bucket.Entries.push_back(std::move(entry));
    if (bucket.Entries.size() >= BatchFileLimit) {
        ReadyQueue.push_back(idx);
        ActiveBuckets.erase(idx);
    }
}
```

### Dispatch

Event-driven from two directions: entries filling buckets and Processors
finishing work.

```cpp
void maybeDispatch() {
    while (AvailableProcs.load() > 0 && !ReadyQueue.empty()) {
        size_t idx = ReadyQueue.front();
        ReadyQueue.pop_front();
        if (idx == RESIDENT_BUCKET_SENTINEL) {
            dispatchResidentBucket();
        } else {
            dispatchBucket(Buckets[idx]);
        }
    }
}

void dispatchBucket(Bucket& bucket) {
    std::sort(bucket.Entries.begin(), bucket.Entries.end(),
        [](const auto& a, const auto& b) {
            return a->DiskOffset < b->DiskOffset;
        });
    // ... move entries, popProc, post to pool
}

void dispatchResidentBucket() {
    std::sort(ResidentBucket.Entries.begin(), ResidentBucket.Entries.end(),
        [](const auto& a, const auto& b) {
            return a->Addr < b->Addr;
        });
    // ... move entries, popProc, post to pool
}
```

`maybeDispatch()` is called after `addToBucket` and when a Processor returns
via `pushProc` (posted to the Strand).

Stale bucket flushing: after each `performScheduling` call, scan
`ActiveBuckets` for buckets exceeding `MaxAge` and push them onto ReadyQueue.

### Processor Availability Tracking

```cpp
std::shared_ptr<Processor> popProc() {
    std::unique_lock<std::mutex> lock(ProcMutex);
    while (Processors.empty()) {
        ProcCV.wait(lock);
    }
    auto proc = Processors.back();
    Processors.pop_back();
    AvailableProcs.fetch_sub(1);
    return proc;
}

void pushProc(const std::shared_ptr<Processor>& proc) {
    {
        std::unique_lock<std::mutex> lock(ProcMutex);
        Processors.push_back(proc);
        AvailableProcs.fetch_add(1);
        ProcCV.notify_one();
    }
    boost::asio::post(Strand, [this]() { maybeDispatch(); });
}
```

## Testing Strategy

### Per-Processor Image Refactor

- **Unit test:** TskEntry::createReadSeek with a mock ReadSeekContext. Verify it
  creates a ReadSeekTSK with the correct inode.
- **Integration test:** Open a test disk image, walk it, have two independent
  Processors process the same entries. Verify identical hash results.

### FileScheduler Bucketing

- **Unit test:** Feed entries with known DiskOffsets into bucketing. Verify
  correct bucket assignment, resident files go to resident bucket, dispatched
  batches are sorted correctly.
- **Unit test:** Dispatch thresholds — full buckets dispatch, stale buckets
  flush, startFilesystem flushes everything.
- **Unit test:** ReadyQueue and ActiveBuckets bookkeeping consistency.

Bucketing unit tests use a stub Entry subclass with fake DiskOffset — no TSK
dependency needed.

### End-to-End

Run against a real disk image and verify output matches the current
implementation. Bucketing changes processing order but not results.
