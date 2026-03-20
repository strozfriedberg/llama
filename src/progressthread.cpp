// ABOUTME: Polls ProgressInfo every 200ms and writes a progress line to stderr
// ABOUTME: Computes rates from deltas between ticks and clears the line on shutdown

#include "progressthread.h"
#include "progressinfo.h"

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
  const auto interval = std::chrono::milliseconds(500);
  auto startTime = Clock::now();

  while (!Info.isDone()) {
    std::this_thread::sleep_for(interval);
    double elapsed = std::chrono::duration<double>(Clock::now() - startTime).count();
    std::string line = Info.formatLine(elapsed);
    std::cerr << "\r\033[K" << line << std::flush;
  }
}
