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
  CurInodes(new InodeBatch())
{
}

void BatchHandler::push(const Dirent& d) {
  CurDents->add(d);
}

void BatchHandler::push(const Inode& i) {
  CurInodes->add(i);
}

void BatchHandler::maybeFlush() {
  if (CurInodes->size() > BATCH_SIZE) {
    flush();
  }
}

void BatchHandler::flush() {
  Sink->scheduleFileBatch(*CurDents, *CurInodes);
  CurDents->clear();
  CurInodes->clear();
}

