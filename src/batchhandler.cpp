#include "batchhandler.h"

#include "filescheduler.h"
#include "duckinode.h"

#include <algorithm>

namespace {
  const unsigned int BATCH_SIZE = 50;
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
  if (entry->FileSize > FileScheduler::LARGE_FILE_THRESHOLD) {
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
  std::sort(CurEntries->begin(), CurEntries->end(),
    [](const std::unique_ptr<Entry>& a, const std::unique_ptr<Entry>& b) {
      return a->DiskOffset < b->DiskOffset;
    });
  Sink->scheduleFileBatch(*CurDents, *CurInodes, CurEntries);
  CurDents->clear();
  CurInodes->clear();
  CurEntries.reset(new std::vector<std::unique_ptr<Entry>>());
}

void BatchHandler::startFilesystem(uint64_t fsSize) {
  flush();
  Sink->startFilesystem(fsSize);
}

void BatchHandler::flushAllBuckets() {
  flush();
  Sink->flushAllBuckets();
}
