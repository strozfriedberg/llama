#pragma once

#include <memory>
#include <vector>

#include <condition_variable>
#include <mutex>

#include "boost_asio.h"

#include "llamaduck.h"
#include "direntbatch.h"
#include "readseek.h"

struct FileRecord;
struct Options;
class OutputHandler;
class Processor;

class InodeBatch;

class FileScheduler {
public:
  FileScheduler(LlamaDB& db, boost::asio::thread_pool& pool,
                const std::shared_ptr<Processor>& protoProc,
                const std::shared_ptr<Options>& opts);

  void scheduleFileBatch(const DirentBatch& dirents,
                         const InodeBatch& inodes,
                         const std::shared_ptr<std::vector<std::unique_ptr<ReadSeek>>>& streams);

  double getProcessorTime();

private:
  void performScheduling(DirentBatch& dirents,
                         InodeBatch& inodes,
                         const std::shared_ptr<std::vector<std::unique_ptr<ReadSeek>>>& streams);

  std::shared_ptr<Processor> popProc();
  void pushProc(const std::shared_ptr<Processor>& proc);

  LlamaDBConnection DBConn;

  boost::asio::thread_pool& Pool;
  boost::asio::strand<boost::asio::thread_pool::executor_type> Strand;

  std::vector<std::shared_ptr<Processor>> Processors;

  std::mutex ProcMutex;
  std::condition_variable ProcCV;
};
