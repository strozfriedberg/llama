#include "filescheduler.h"

#include "direntbatch.h"
#include "filerecord.h"
#include "duckinode.h"
#include "options.h"
#include "outputhandler.h"
#include "processor.h"

#include <random>

namespace {
  std::string randomNumString() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 999999999);
    return std::to_string(dis(gen));
  }
}

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
                                      std::unique_ptr<std::vector<std::unique_ptr<ReadSeek>>> streams)
{
  // here we copy the batches in the lambda capture, so that the passed-in batches
  // can be reused by the caller while the scheduler does its work on a separate thread
  auto dPtr = std::make_shared<DirentBatch>(dirents);
  auto iPtr = std::make_shared<InodeBatch>(inodes);
  boost::asio::post(
    Strand,
    [=, streams = std::move(streams)]() {
      performScheduling(*dPtr, *iPtr, std::move(streams));
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
                                      std::unique_ptr<std::vector<std::unique_ptr<ReadSeek>>> streams)
{
  std::string tmpDents = "_temp_dirent";
  std::string tmpInodes = "_temp_inode";
  //std::string batchTbl = "_temp_batch_" + randomNumString();

  DuckDirent::createTable(DBConn.get(), tmpDents);
  DuckInode::createTable(DBConn.get(), tmpInodes);

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
  boost::asio::post(Pool, [=]() {
/*    for (auto& rec : *batch) {
      proc->process(rec, *Output);
    }*/
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

