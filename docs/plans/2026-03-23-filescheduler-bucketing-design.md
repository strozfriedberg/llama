# FileScheduler Bucketing Design

## Problem

After the per-Processor TSK image refactor, wall clock for the nfury image
dropped from 97s to 28s. Switching from Blake3 to SHA-256 brought it to 26s.
CPU time decreased but wall clock gains were modest because load is lumpy:
worker threads are stalled >50% of their time reading data, and batches vary
wildly in I/O cost. At end-of-run, the TSK walk finishes and the orphan scan
produces a burst of files that only a few Processors work through, creating a
straggler tail.

Bucketing groups files by disk location so each Processor's reads are
sequential within the libewf chunk cache, reducing decompression misses and
improving throughput. Byte-aware dispatch prevents a single batch from
accumulating disproportionate I/O cost.

## Data Flow

The walk produces entries one at a time through TSK callbacks. Each entry
arrives at BatchHandler, which makes a size-based decision:

**Large files (>256MB):** Flush the current dirent/inode batch to DuckDB
(ensuring DB consistency before any Processor touches the entry), then send the
entry directly to FileScheduler, which wraps it in a single-entry bucket and
pushes it onto the priority queue.

**Normal files:** Buffer in BatchHandler as today. When the buffer reaches ~50
entries, sort the entries by disk offset, flush dirents/inodes to DuckDB, then
send the sorted entries to FileScheduler for bucketing.

FileScheduler maintains a vector of buckets, one per 256MB span of the current
filesystem. Incoming entries are placed in the bucket corresponding to their
first data run's disk offset. Resident files (no disk offset) go to a dedicated
resident bucket.

When a bucket's accumulated file sizes reach 256MB, it is moved onto a priority
queue ordered by total bytes (largest first). When a Processor returns to the
pool, FileScheduler pops the largest bucket from the queue. The Processor sorts
the batch by disk offset and processes it.

Buckets are flushed at two points: when a new filesystem begins (flush previous
filesystem's buckets) and when the entire walk completes including orphans
(flush everything remaining). Flushing pushes all non-empty buckets onto the
priority queue regardless of whether they have reached the byte threshold.

## Bucket Structure

```cpp
struct Bucket {
    std::vector<std::unique_ptr<Entry>> Entries;
    uint64_t TotalBytes = 0;

    bool ready() const { return TotalBytes >= BUCKET_BYTE_LIMIT; }
    bool empty() const { return Entries.empty(); }
};
```

## FileScheduler Changes

```cpp
static constexpr uint64_t BUCKET_SPAN = 256 * 1024 * 1024;
static constexpr uint64_t BUCKET_BYTE_LIMIT = BUCKET_SPAN;

std::vector<Bucket> Buckets;
Bucket ResidentBucket;

// Largest-bytes-first dispatch queue
std::priority_queue<Bucket, std::vector<Bucket>,
    /* TotalBytes ascending comparator so largest is on top */> DispatchQueue;
```

Per-filesystem lifecycle:

```cpp
void startFilesystem(uint64_t fsSize) {
    flushAllBuckets();
    size_t numBuckets = (fsSize + BUCKET_SPAN - 1) / BUCKET_SPAN;
    Buckets.clear();
    Buckets.resize(numBuckets);
    ResidentBucket = Bucket{};
}
```

Entry distribution is O(1): compute `diskOffset / BUCKET_SPAN` to get the
bucket index. When a bucket crosses the byte threshold, move it onto the
dispatch queue. Because BatchHandler sorts entries by disk offset before
sending, FileScheduler accesses the bucket vector sequentially.

When a Processor returns to the pool, pop the top bucket from the dispatch
queue and hand it to the Processor. The Processor sorts the batch by disk
offset before processing. If the queue is empty, the Processor waits in the
pool.

## BatchHandler Changes

```cpp
static constexpr uint64_t LARGE_FILE_THRESHOLD = 256 * 1024 * 1024;
```

When an entry arrives from the walk callback:

- If `entry->FileSize > LARGE_FILE_THRESHOLD`: flush the current dirent/inode
  batch to DuckDB, then send the single entry to FileScheduler (which pushes it
  onto the priority queue as a single-entry bucket).
- Otherwise: buffer as today.

When the buffer reaches ~50 entries, sort by disk offset, flush dirents/inodes
to DuckDB, send entries to FileScheduler for bucketing.

## Batch Tracking Table

FileScheduler writes a row to a DuckDB `batches` table for each dispatched
batch, enabling post-run analysis of scheduling behavior:

| Column      | Type     | Description                                      |
|-------------|----------|--------------------------------------------------|
| BatchId     | uint64   | Sequential batch identifier                      |
| BucketIndex | int64    | Disk region index (-1 for resident, -2 for large)|
| NumEntries  | uint64   | Number of files in the batch                     |
| TotalBytes  | uint64   | Sum of file sizes in the batch                   |

## Testing

**Bucket distribution:** Feed entries with known disk offsets and file sizes
into FileScheduler. Verify entries land in the correct bucket, resident files
go to the resident bucket, and buckets enter the priority queue when they cross
the byte threshold.

**Priority queue ordering:** Push multiple buckets with different byte totals.
Verify dispatch order is largest-first.

**Large file bypass:** Send a >256MB entry through BatchHandler. Verify
dirent/inode flush happens immediately and entry reaches the priority queue as
a single-entry bucket.

**Filesystem transition:** Populate buckets across two filesystems. Verify
`startFilesystem()` flushes all previous buckets onto the priority queue before
allocating new ones.

**End-of-walk flush:** Leave partial buckets below threshold and verify
`flushAllBuckets()` pushes them all onto the priority queue.

**Batch sorting in Processor:** Give Processor a batch with entries in random
disk-offset order. Verify it sorts them before processing.

All bucketing tests use stub entries with fake disk offsets and file sizes — no
TSK dependency.

**End-to-end:** Run against a real disk image and verify output matches the
current implementation. Bucketing changes processing order but not results.
