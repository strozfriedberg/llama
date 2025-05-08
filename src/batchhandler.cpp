#include "batchhandler.h"

#include "filescheduler.h"
#include "duckinode.h"

namespace {
  const unsigned int BATCH_SIZE = 5000;
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
  CurEntries->push_back(std::move(entry));
}

void BatchHandler::maybeFlush() {
  if (CurInodes->size() > BATCH_SIZE) {
    flush();
  }
}

void BatchHandler::flush() {
  Sink->scheduleFileBatch(*CurDents, *CurInodes, CurEntries);
  CurDents->clear();
  CurInodes->clear();
  CurEntries.reset(new std::vector<std::unique_ptr<Entry>>());
}
