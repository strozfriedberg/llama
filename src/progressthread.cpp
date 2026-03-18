// ABOUTME: Polls ProgressInfo every 200ms and writes a progress line to stderr
// ABOUTME: Computes rates from deltas between ticks and clears the line on shutdown

#include "progressthread.h"
#include "progressinfo.h"

#include <chrono>
#include <iostream>

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define STDERR_FILENO 2
#else
#include <unistd.h>
#endif

ProgressThread::ProgressThread(ProgressInfo& info, bool isTty)
  : Info(info), IsTty(isTty) {}

void ProgressThread::start() {
  if (!IsTty) {
    return;
  }
  Thread = std::thread(&ProgressThread::run, this);
}

void ProgressThread::stop() {
  if (Thread.joinable()) {
    Thread.join();
    // Clear the progress line
    std::cerr << "\r\033[K" << std::flush;
  }
}

void ProgressThread::run() {
  using Clock = std::chrono::steady_clock;
  const auto interval = std::chrono::milliseconds(200);
  auto startTime = Clock::now();
  uint64_t prevInodes = 0;
  uint64_t prevBytes = 0;
  auto prevTime = startTime;

  while (!Info.isDone()) {
    std::this_thread::sleep_for(interval);

    auto now = Clock::now();
    double elapsed = std::chrono::duration<double>(now - startTime).count();
    double dt = std::chrono::duration<double>(now - prevTime).count();

    uint64_t curInodes = Info.inodesProcessed();
    uint64_t curBytes = Info.bytesProcessed();

    double inodesPerSec = (dt > 0) ? (curInodes - prevInodes) / dt : 0;
    double bytesPerSec = (dt > 0) ? (curBytes - prevBytes) / dt : 0;

    std::string line = Info.formatLine(elapsed, inodesPerSec, bytesPerSec);
    std::cerr << "\r\033[K" << line << std::flush;

    prevInodes = curInodes;
    prevBytes = curBytes;
    prevTime = now;
  }
}
