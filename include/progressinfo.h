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
  void setFilesystem(uint32_t index, uint32_t total, uint64_t inodeCount, uint64_t totalBytes);
  void setDone();
  void addException();

  uint64_t inodesProcessed() const;
  uint64_t bytesProcessed() const;
  uint32_t filesystemIndex() const;
  uint32_t filesystemCount() const;
  uint64_t inodeCount() const;
  uint64_t totalBytes() const;
  bool isDone() const;
  uint64_t exceptionCount() const;

  // Format the progress line for display.
  // Rates are computed as cumulative averages from elapsed time.
  std::string formatLine(double elapsedSecs) const;

private:
  std::atomic<uint64_t> InodesProcessed;
  std::atomic<uint64_t> BytesProcessed;
  std::atomic<uint32_t> FilesystemIndex;
  std::atomic<uint32_t> FilesystemCount;
  std::atomic<uint64_t> InodeCount;
  std::atomic<uint64_t> TotalBytes;
  std::atomic<bool> Done;
  std::atomic<uint64_t> ExceptionCount;
};
