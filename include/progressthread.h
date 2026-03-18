// ABOUTME: Dedicated thread that polls ProgressInfo and prints a progress line to stderr
// ABOUTME: Updates every 200ms, only runs when stderr is a terminal

#pragma once

#include <thread>

class ProgressInfo;

class ProgressThread {
public:
  // If isTty is true, the thread will print progress to stderr.
  // If false, the thread does nothing (for non-terminal or testing).
  ProgressThread(ProgressInfo& info, bool isTty);

  void start();
  void stop();

  ProgressThread(const ProgressThread&) = delete;
  ProgressThread& operator=(const ProgressThread&) = delete;

private:
  void run();

  ProgressInfo& Info;
  bool IsTty;
  std::thread Thread;
};
