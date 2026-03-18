// ABOUTME: Thread-safe progress counter implementation
// ABOUTME: Formats a single-line progress display for stderr

#include "progressinfo.h"

#include <cstdio>
#include <sstream>

ProgressInfo::ProgressInfo()
  : InodesProcessed(0), BytesProcessed(0),
    FilesystemIndex(0), FilesystemCount(0),
    InodeCount(0), Done(false) {}

void ProgressInfo::update(uint64_t inodes, uint64_t bytes) {
  InodesProcessed.fetch_add(inodes, std::memory_order_relaxed);
  BytesProcessed.fetch_add(bytes, std::memory_order_relaxed);
}

void ProgressInfo::setFilesystem(uint32_t index, uint32_t total, uint64_t inodeCount) {
  InodesProcessed.store(0, std::memory_order_relaxed);
  BytesProcessed.store(0, std::memory_order_relaxed);
  FilesystemIndex.store(index, std::memory_order_relaxed);
  FilesystemCount.store(total, std::memory_order_relaxed);
  InodeCount.store(inodeCount, std::memory_order_relaxed);
}

void ProgressInfo::setDone() {
  Done.store(true, std::memory_order_release);
}

uint64_t ProgressInfo::inodesProcessed() const {
  return InodesProcessed.load(std::memory_order_relaxed);
}

uint64_t ProgressInfo::bytesProcessed() const {
  return BytesProcessed.load(std::memory_order_relaxed);
}

uint32_t ProgressInfo::filesystemIndex() const {
  return FilesystemIndex.load(std::memory_order_relaxed);
}

uint32_t ProgressInfo::filesystemCount() const {
  return FilesystemCount.load(std::memory_order_relaxed);
}

uint64_t ProgressInfo::inodeCount() const {
  return InodeCount.load(std::memory_order_relaxed);
}

bool ProgressInfo::isDone() const {
  return Done.load(std::memory_order_acquire);
}

namespace {
  std::string formatWithCommas(uint64_t value) {
    std::string s = std::to_string(value);
    int insertPos = static_cast<int>(s.length()) - 3;
    while (insertPos > 0) {
      s.insert(insertPos, ",");
      insertPos -= 3;
    }
    return s;
  }

  std::string formatBytes(double bytes) {
    if (bytes >= 1024.0 * 1024.0 * 1024.0) {
      char buf[32];
      std::snprintf(buf, sizeof(buf), "%.1f GB/s", bytes / (1024.0 * 1024.0 * 1024.0));
      return buf;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f MB/s", bytes / (1024.0 * 1024.0));
    return buf;
  }

  std::string formatElapsed(double secs) {
    int total = static_cast<int>(secs);
    int h = total / 3600;
    int m = (total % 3600) / 60;
    int s = total % 60;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d:%02d:%02d", h, m, s);
    return buf;
  }
}

std::string ProgressInfo::formatLine(double elapsedSecs, double inodesPerSec, double bytesPerSec) const {
  std::ostringstream oss;
  uint64_t inodes = inodesProcessed();
  uint64_t total = inodeCount();
  uint32_t fsCount = filesystemCount();

  if (total > 0) {
    uint32_t pct = static_cast<uint32_t>(inodes * 100 / total);
    oss << "Filesystem " << filesystemIndex();
    if (fsCount > 0) {
      oss << "/" << fsCount;
    }
    oss << ": " << pct << "% | ";
  }

  char rateStr[32];
  std::snprintf(rateStr, sizeof(rateStr), "%.0f", inodesPerSec);

  oss << formatWithCommas(inodes) << " inodes | "
      << rateStr << " files/s | "
      << formatBytes(bytesPerSec) << " | "
      << formatElapsed(elapsedSecs);

  return oss.str();
}
