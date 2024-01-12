#include "filerecord.h"
#include "outputwriter.h"
#include "pooloutputhandler.h"

PoolOutputHandler::PoolOutputHandler(boost::asio::thread_pool& pool, std::shared_ptr<OutputWriter> out):
  MainStrand(pool.get_executor()),
  RecStrand(pool.get_executor()),
  Out(out),
  ImageRecBuf("recs/image", 4 * 1024, [this](const OutputChunk& c) { Out->outputImage(c); }),
  InodesRecBuf("recs/inodes", 16 * 1024 * 1024, [this](const OutputChunk& c) { Out->outputInode(c); }),
  DirentsRecBuf("recs/dirents", 16 * 1024 * 1024, [this](const OutputChunk& c) { Out->outputDirent(c); }),
  Closed(false)
{}

PoolOutputHandler::~PoolOutputHandler() {
  close();
}

void PoolOutputHandler::outputImage(const FileRecord& rec) {
  ImageRecBuf.write(rec.str());
}

void PoolOutputHandler::outputDirent(const FileRecord& rec) {
/*
  if (Closed) {
    // we might still have some records in FileRecBuf, but the
    // threadpool has gone away and the MainStrand can no longer be
    // posted to, so just call into the function directly.
    Out->outputDirent(rec);
  }
  else {
    boost::asio::post(MainStrand, [=](){
      Out->outputDirent(rec);
    });
  }
*/
  boost::asio::post(RecStrand, [=]() {
    DirentsRecBuf.write(rec.str());
  });
}

void PoolOutputHandler::outputInode(const FileRecord& rec) {
  boost::asio::post(RecStrand, [=]() {
    InodesRecBuf.write(rec.str());
  });
}

void PoolOutputHandler::outputInodes(const std::shared_ptr<std::vector<FileRecord>>& batch) {
  boost::asio::post(RecStrand, [=]() {
    for (const auto& rec: *batch) {
      InodesRecBuf.write(rec.str());
      // FileRecBuf.get() << rec.Doc << '\n';
    }
  });
}

void PoolOutputHandler::outputSearchHit(const std::string&) {
}

void PoolOutputHandler::outputSearchHits(const std::vector<std::string>& batch) {
  for (const auto& hit: batch) {
    outputSearchHit(hit);
  }
}

void PoolOutputHandler::close() {
  Closed = true;

  if (ImageRecBuf.size()) {
    ImageRecBuf.flush();
  }

  if (DirentsRecBuf.size()) {
    DirentsRecBuf.flush();
  }

  if (InodesRecBuf.size()) {
    InodesRecBuf.flush();
  }

  Out->close();
}