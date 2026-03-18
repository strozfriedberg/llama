// ABOUTME: Thread-safe counters for tracking processing progress
// ABOUTME: Provides atomic update/query interface and progress line formatting

#pragma once

#include <atomic>
#include <cstdint>
#include <string>

class ProgressInfo {
public:
  ProgressInfo();

  void update(uint64_t inodes, uint64_t bytes);
  void setFilesystem(uint32_t index, uint32_t total, uint64_t inodeCount);
  void setDone();

  uint64_t inodesProcessed() const;
  uint64_t bytesProcessed() const;
  uint32_t filesystemIndex() const;
  uint32_t filesystemCount() const;
  uint64_t inodeCount() const;
  bool isDone() const;

  // Format the progress line for display.
  // inodesPerSec and bytesPerSec are rates computed by the caller.
  // elapsedSecs is wall-clock time since processing started.
  std::string formatLine(double elapsedSecs, double inodesPerSec, double bytesPerSec) const;

private:
  std::atomic<uint64_t> InodesProcessed;
  std::atomic<uint64_t> BytesProcessed;
  std::atomic<uint32_t> FilesystemIndex;
  std::atomic<uint32_t> FilesystemCount;
  std::atomic<uint64_t> InodeCount;
  std::atomic<bool> Done;
};
