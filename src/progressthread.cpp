// ABOUTME: Polls ProgressInfo every 200ms and writes a progress line to stderr
// ABOUTME: Computes rates from deltas between ticks and clears the line on shutdown

#include "progressthread.h"
#include "progressinfo.h"

#include <algorithm>
#include <chrono>
#include <iostream>

ProgressThread::ProgressThread(ProgressInfo& info, bool isTty)
  : Info(info), IsTty(isTty) {}

void ProgressThread::start() {
  if (!IsTty) {
    return;
  }
  Thread = std::thread(&ProgressThread::run, this);
}

void ProgressThread::stop() {
  Info.setDone();
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

    // std::max clamps to 0 when counters reset at filesystem boundaries
    double inodeDelta = static_cast<double>(std::max(curInodes, prevInodes) - prevInodes);
    double bytesDelta = static_cast<double>(std::max(curBytes, prevBytes) - prevBytes);

    std::string line = Info.formatLine(elapsed, inodeDelta / dt, bytesDelta / dt);
    std::cerr << "\r\033[K" << line << std::flush;

    prevInodes = curInodes;
    prevBytes = curBytes;
    prevTime = now;
  }
}
