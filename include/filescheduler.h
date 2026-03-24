// ABOUTME: Schedules file processing batches across a thread pool with disk-locality bucketing.
// ABOUTME: Groups entries into 256MB disk-region buckets and dispatches via a priority queue.

#pragma once

#include <memory>
#include <queue>
#include <vector>

#include <condition_variable>
#include <mutex>

#include "boost_asio.h"

#include "entry.h"
#include "llamaduck.h"
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

  double getProcessorTime();

private:
  void performScheduling(DirentBatch& dirents,
                         InodeBatch& inodes,
                         const std::shared_ptr<std::vector<std::unique_ptr<Entry>>>& entries);

  std::shared_ptr<Processor> popProc();
  void pushProc(const std::shared_ptr<Processor>& proc);

  LlamaDBConnection DBConn;

  boost::asio::thread_pool& Pool;
  boost::asio::strand<boost::asio::thread_pool::executor_type> Strand;

  std::vector<std::shared_ptr<Processor>> Processors;

  std::mutex ProcMutex;
  std::condition_variable ProcCV;
};
