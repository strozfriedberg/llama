#pragma once

#include "boost_asio.h"
//#include "direntbatch.h"
//#include "duckinode.h"
//#include "llamaduck.h"
#include "outputhandler.h"
#include "recordbuffer.h"

struct OutputChunk;
class OutputWriter;

class PoolOutputHandler: public OutputHandler {
public:
  PoolOutputHandler(boost::asio::thread_pool& pool, std::shared_ptr<OutputWriter> out);

  virtual ~PoolOutputHandler();

  virtual void outputImage(const FileRecord& rec) override;

  virtual void outputDirent(const Dirent&) override {}

  virtual void outputInode(const FileRecord&) override {}
  virtual void outputInode(const Inode&) override {}

  virtual void outputInodes(const std::shared_ptr<std::vector<FileRecord>>& batch) override;

  virtual void outputSearchHit(const std::string&) override;

  virtual void outputSearchHits(const std::vector<std::string>& batch) override;

  virtual void close() override;

private:
  boost::asio::strand<boost::asio::thread_pool::executor_type> MainStrand,
                                                               RecStrand;

//  LlamaDBAppender DirentAppender;
//  LlamaDBAppender InodeAppender;

  RecordBuffer ImageRecBuf;
  RecordBuffer InodesRecBuf;
  
//  DirentBatch DirentsBatch;
//  InodeBatch  InodesBatch;

  std::shared_ptr<OutputWriter> Out;

  bool Closed;
};
