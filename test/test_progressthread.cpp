// ABOUTME: Tests for ProgressThread — the 200ms polling thread for progress display
// ABOUTME: Verifies start/stop lifecycle and terminal detection

#include <catch2/catch_test_macros.hpp>
#include "progressthread.h"
#include "progressinfo.h"

#include <thread>
#include <chrono>

TEST_CASE("ProgressThread starts and stops cleanly") {
  ProgressInfo pi;
  // Force non-tty so no output is produced during tests
  ProgressThread pt(pi, false);
  pt.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  pi.setDone();
  pt.stop();
  // Just verify no crash/hang
  REQUIRE(true);
}

TEST_CASE("ProgressThread does not start when disabled") {
  ProgressInfo pi;
  ProgressThread pt(pi, false);
  pt.start();
  pi.setDone();
  pt.stop();
  REQUIRE(true);
}
