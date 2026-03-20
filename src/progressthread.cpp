// ABOUTME: Polls ProgressInfo every 200ms and writes a progress line to stderr
// ABOUTME: Prints final stats on shutdown so they persist in the terminal

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
    // Print final stats with newline so they persist
    double elapsed = std::chrono::duration<double>(Clock::now() - StartTime).count();
    std::cerr << "\r\033[K" << Info.formatLine(elapsed) << std::endl;
  }
}

void ProgressThread::run() {
  const auto interval = std::chrono::milliseconds(200);
  StartTime = Clock::now();

  while (!Info.isDone()) {
    std::this_thread::sleep_for(interval);
    double elapsed = std::chrono::duration<double>(Clock::now() - StartTime).count();
    std::cerr << "\r\033[K" << Info.formatLine(elapsed) << std::flush;
  }
}
