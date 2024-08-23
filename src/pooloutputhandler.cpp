#include "filerecord.h"
#include "llamaduck.h"
#include "outputwriter.h"
#include "pooloutputhandler.h"

#include <iostream>

PoolOutputHandler::PoolOutputHandler(boost::asio::thread_pool& pool, LlamaDBConnection& conn, std::shared_ptr<OutputWriter> out):
  MainStrand(pool.get_executor()),
  RecStrand(pool.get_executor()),
  DirentAppender(conn.get(), "dirent"),
  InodeAppender(conn.get(), "inode"),
  ImageRecBuf("recs/image", 4 * 1024, [this](const OutputChunk& c) { Out->outputImage(c); }),
  InodesRecBuf("recs/inodes", 16 * 1024 * 1024, [this](const OutputChunk& c) { Out->outputInode(c); }),
  Out(out),
  Closed(false)
{}

PoolOutputHandler::~PoolOutputHandler() {
  close();
}

void PoolOutputHandler::outputImage(const FileRecord& rec) {
  ImageRecBuf.write(rec.str());
}

void PoolOutputHandler::outputDirent(const Dirent& rec) {
  boost::asio::post(RecStrand, [&, rec]() {
    DirentsBatch.add(rec);
    if (DirentsBatch.size() >= 1000) {
      DirentsBatch.copyToDB(DirentAppender.get());
      DirentAppender.flush();
    }
  });
}

void PoolOutputHandler::outputInode(const FileRecord& rec) {
  boost::asio::post(RecStrand, [=]() {
    InodesRecBuf.write(rec.str());
  });
}

void PoolOutputHandler::outputInode(const Inode& rec) {
  boost::asio::post(RecStrand, [&, rec]() {
    InodesBatch.add(rec);
    if (InodesBatch.size() >= 1000) {
      InodesBatch.copyToDB(InodeAppender.get());
      InodeAppender.flush();
    }
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

  if (InodesRecBuf.size()) {
    InodesRecBuf.flush();
  }
  if (DirentsBatch.size()) {
    DirentsBatch.copyToDB(DirentAppender.get());
    DirentAppender.flush();
  //  std::cerr << "wrote " << num << " dirents\n";
  }
  if (InodesBatch.size()) {
    InodesBatch.copyToDB(InodeAppender.get());
    InodeAppender.flush();
  }
  Out->close();
}
