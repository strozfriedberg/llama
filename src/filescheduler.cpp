// ABOUTME: Implements FileScheduler batch dispatch and BucketState disk-locality bucketing.
// ABOUTME: Distributes entries into 256MB disk-region buckets with priority queue dispatch.

#include "filescheduler.h"

#include "direntbatch.h"
#include "filerecord.h"
#include "duckinode.h"
#include "options.h"
#include "outputhandler.h"
#include "processor.h"


FileScheduler::FileScheduler(LlamaDB& db,
                             boost::asio::thread_pool& pool,
                             const std::shared_ptr<Processor>& protoProc,
                             const std::shared_ptr<Options>& opts)
    : DBConn(db), Pool(pool), Strand(Pool.get_executor()),
      ProcMutex(), ProcCV() {
  for (unsigned int i = 0; i < opts->NumThreads; ++i) {
    Processors.push_back(protoProc->clone());
  }
}

void FileScheduler::scheduleFileBatch(const DirentBatch& dirents,
                                      const InodeBatch& inodes,
                                      const std::shared_ptr<std::vector<std::unique_ptr<Entry>>>& entries)
{
  // here we copy the batches in the lambda capture, so that the passed-in batches
  // can be reused by the caller while the scheduler does its work on a separate thread
  auto dPtr = std::make_shared<DirentBatch>(dirents);
  auto iPtr = std::make_shared<InodeBatch>(inodes);
  boost::asio::post(
    Strand,
    [=, this]() {
      performScheduling(*dPtr, *iPtr, entries);
    }
  );
}

double FileScheduler::getProcessorTime() {
  double ret = 0;
  for (auto& p : Processors) {
    ret += p->getProcessorTime();
  }
  return ret;
}

void FileScheduler::performScheduling(DirentBatch& dirents,
                                      InodeBatch& inodes,
                                      const std::shared_ptr<std::vector<std::unique_ptr<Entry>>>& entries)
{
  std::string tmpDents = "_temp_dirent";
  std::string tmpInodes = "_temp_inode";
  //std::string batchTbl = "_temp_batch_" + randomNumString();

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

  // post for multithreaded processing
  auto proc = popProc(); // blocks
  boost::asio::post(Pool, [=, this]() {
    proc->processBatch(entries);
    this->pushProc(proc);
  });
}

std::shared_ptr<Processor> FileScheduler::popProc() {
  // Having a fixed number of Processor objects adds back pressure to
  // FileScheduler here -- it cannot dispatch more batches beyond the
  // size of the Processor pool.
  std::unique_lock<std::mutex> lock(ProcMutex);
  while (Processors.empty()) {
    // Releases mutex inside wait(), but reacquires before returning
    ProcCV.wait(lock);
  }
  auto batterUp = Processors.back();
  Processors.pop_back();
  return batterUp;
}

void FileScheduler::pushProc(const std::shared_ptr<Processor>& proc) {
  std::unique_lock<std::mutex> lock(ProcMutex);
  Processors.push_back(proc);
  ProcCV.notify_one();
}

// BucketState implementation

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

