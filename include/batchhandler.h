#include "inputhandler.h"

#include <memory>

class FileScheduler;

class DirentBatch;
class InodeBatch;

class BatchHandler: public InputHandler {
public:
  BatchHandler(std::shared_ptr<FileScheduler> sink);

  virtual ~BatchHandler() {
    flush();
  }

  virtual void push(const Dirent& dirent) override;
  virtual void push(const Inode& inode) override;

  virtual void maybeFlush() override; // flushes only if the batch is full
  virtual void flush() override; // always flushes

private:
  std::shared_ptr<FileScheduler> Sink;

  std::unique_ptr<DirentBatch> CurDents;
  std::unique_ptr<InodeBatch>  CurInodes;
};

