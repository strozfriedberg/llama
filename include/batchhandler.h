#include "inputhandler.h"

#include <memory>
#include <vector>

#include "direntbatch.h"
#include "duckinode.h"
#include "readseek.h"

class FileScheduler;

class BatchHandler: public InputHandler {
public:
  BatchHandler(std::shared_ptr<FileScheduler> sink);

  virtual ~BatchHandler() {
    flush();
  }

  virtual void push(const Dirent& dirent) override;
  virtual void push(const Inode& inode) override;
  virtual void push(std::unique_ptr<ReadSeek> stream) override;

  virtual void maybeFlush() override; // flushes only if the batch is full
  virtual void flush() override; // always flushes

private:
  std::shared_ptr<FileScheduler> Sink;

  std::unique_ptr<DirentBatch> CurDents;
  std::unique_ptr<InodeBatch>  CurInodes;
  std::shared_ptr<std::vector<std::unique_ptr<ReadSeek>>> CurStreams;
};

