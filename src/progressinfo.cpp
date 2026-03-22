// ABOUTME: Thread-safe progress counter implementation
// ABOUTME: Formats a single-line progress display for stderr

#include "progressinfo.h"

#include <cstdio>

ProgressInfo::ProgressInfo()
  : InodesProcessed(0), BytesProcessed(0),
    FilesystemIndex(0), FilesystemCount(0),
    InodeCount(0), TotalBytes(0), Done(false), ExceptionCount(0) {}

void ProgressInfo::update(uint64_t inodes, uint64_t bytes) {
  InodesProcessed.fetch_add(inodes);
  BytesProcessed.fetch_add(bytes);
}

void ProgressInfo::setFilesystem(uint32_t index, uint32_t total, uint64_t inodeCount, uint64_t totalBytes) {
  InodesProcessed.store(0);
  BytesProcessed.store(0);
  FilesystemIndex.store(index);
  FilesystemCount.store(total);
  InodeCount.store(inodeCount);
  TotalBytes.store(totalBytes);
}

void ProgressInfo::setDone() {
  Done.store(true);
}

uint64_t ProgressInfo::inodesProcessed() const {
  return InodesProcessed.load();
}

uint64_t ProgressInfo::bytesProcessed() const {
  return BytesProcessed.load();
}

uint32_t ProgressInfo::filesystemIndex() const {
  return FilesystemIndex.load();
}

uint32_t ProgressInfo::filesystemCount() const {
  return FilesystemCount.load();
}

uint64_t ProgressInfo::inodeCount() const {
  return InodeCount.load();
}

uint64_t ProgressInfo::totalBytes() const {
  return TotalBytes.load();
}

bool ProgressInfo::isDone() const {
  return Done.load();
}

void ProgressInfo::addException() {
  ExceptionCount.fetch_add(1);
}

uint64_t ProgressInfo::exceptionCount() const {
  return ExceptionCount.load();
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

  // Format a byte count as a human-readable size (KB, MB, GB, TB)
  std::string formatSize(uint64_t bytes) {
    char buf[32];
    double val = static_cast<double>(bytes);
    if (val >= 1024.0 * 1024.0 * 1024.0 * 1024.0) {
      std::snprintf(buf, sizeof(buf), "%.1f TB", val / (1024.0 * 1024.0 * 1024.0 * 1024.0));
    }
    else if (val >= 1024.0 * 1024.0 * 1024.0) {
      std::snprintf(buf, sizeof(buf), "%.1f GB", val / (1024.0 * 1024.0 * 1024.0));
    }
    else if (val >= 1024.0 * 1024.0) {
      std::snprintf(buf, sizeof(buf), "%.1f MB", val / (1024.0 * 1024.0));
    }
    else if (val >= 1024.0) {
      std::snprintf(buf, sizeof(buf), "%.1f KB", val / 1024.0);
    }
    else {
      std::snprintf(buf, sizeof(buf), "%llu B", static_cast<unsigned long long>(bytes));
    }
    return buf;
  }

  // Format a byte rate as human-readable (MB/s, GB/s, etc.)
  std::string formatRate(double bytesPerSec) {
    char buf[32];
    if (bytesPerSec >= 1024.0 * 1024.0 * 1024.0) {
      std::snprintf(buf, sizeof(buf), "%.1f GB/s", bytesPerSec / (1024.0 * 1024.0 * 1024.0));
    }
    else {
      std::snprintf(buf, sizeof(buf), "%.1f MB/s", bytesPerSec / (1024.0 * 1024.0));
    }
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

std::string ProgressInfo::formatLine(double elapsedSecs) const {
  uint64_t inodes = inodesProcessed();
  uint64_t total = inodeCount();
  uint64_t procBytes = bytesProcessed();
  uint64_t totBytes = totalBytes();
  uint32_t fsIdx = filesystemIndex();
  uint32_t fsCount = filesystemCount();

  double inodesPerSec = (elapsedSecs > 0) ? static_cast<double>(inodes) / elapsedSecs : 0;
  double bytesPerSec = (elapsedSecs > 0) ? static_cast<double>(procBytes) / elapsedSecs : 0;

  char buf[256];
  char* p = buf;
  char* end = buf + sizeof(buf);

  // Filesystem header with percentage
  if (total > 0) {
    uint32_t pct = static_cast<uint32_t>(inodes * 100 / total);
    if (fsCount > 0) {
      p += std::snprintf(p, end - p, "Filesystem %u/%u: %3u%% | ", fsIdx, fsCount, pct);
    }
    else {
      p += std::snprintf(p, end - p, "Filesystem %u: %3u%% | ", fsIdx, pct);
    }
  }

  // Inodes: processed/total
  if (total > 0) {
    p += std::snprintf(p, end - p, "%11s/%s inodes | ",
                       formatWithCommas(inodes).c_str(),
                       formatWithCommas(total).c_str());
  }
  else {
    p += std::snprintf(p, end - p, "%11s inodes | ",
                       formatWithCommas(inodes).c_str());
  }

  // Bytes: processed/total
  if (totBytes > 0) {
    p += std::snprintf(p, end - p, "%9s/%s | ",
                       formatSize(procBytes).c_str(),
                       formatSize(totBytes).c_str());
  }
  else {
    p += std::snprintf(p, end - p, "%9s | ",
                       formatSize(procBytes).c_str());
  }

  // Rates and elapsed
  p += std::snprintf(p, end - p, "%7s files/s | %10s | %s",
                     formatWithCommas(static_cast<uint64_t>(inodesPerSec)).c_str(),
                     formatRate(bytesPerSec).c_str(),
                     formatElapsed(elapsedSecs).c_str());

  uint64_t exceptions = exceptionCount();
  if (exceptions > 0) {
    p += std::snprintf(p, end - p, " | %s exceptions",
                       formatWithCommas(exceptions).c_str());
  }

  return std::string(buf);
}
