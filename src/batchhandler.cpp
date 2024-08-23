#include "batchhandler.h"

#include "direntbatch.h"
#include "filescheduler.h"
#include "duckinode.h"

namespace {
  const unsigned int BATCH_SIZE = 5000;
}

BatchHandler::BatchHandler(std::shared_ptr<FileScheduler> sink):
  Sink(sink),
  CurDents(new DirentBatch()),
  CurInodes(new InodeBatch()),
  CurStreams(new std::vector<std::unique_ptr<ReadSeek>>())
{
}

void BatchHandler::push(const Dirent& d) {
  CurDents->add(d);
}

void BatchHandler::push(const Inode& i) {
  CurInodes->add(i);
}

void BatchHandler::push(std::unique_ptr<ReadSeek> stream) {
  CurStreams->push_back(std::move(stream));
}

void BatchHandler::maybeFlush() {
  if (CurInodes->size() > BATCH_SIZE) {
    flush();
  }
}

void BatchHandler::flush() {
  Sink->scheduleFileBatch(*CurDents, *CurInodes, CurStreams);
  CurDents->clear();
  CurInodes->clear();
  CurStreams.reset(new std::vector<std::unique_ptr<ReadSeek>>());
}

