// ABOUTME: Tests for ProgressInfo atomic counter class
// ABOUTME: Verifies update(), setFilesystem(), and formatting logic

#include <catch2/catch_test_macros.hpp>
#include "progressinfo.h"

TEST_CASE("ProgressInfo update increments counters") {
  ProgressInfo pi;
  pi.update(10, 1024);
  pi.update(5, 512);
  REQUIRE(pi.inodesProcessed() == 15);
  REQUIRE(pi.bytesProcessed() == 1536);
}

TEST_CASE("ProgressInfo setFilesystem resets counters") {
  ProgressInfo pi;
  pi.update(10, 1024);
  pi.setFilesystem(2, 3, 50000);
  REQUIRE(pi.inodesProcessed() == 0);
  REQUIRE(pi.bytesProcessed() == 0);
  REQUIRE(pi.filesystemIndex() == 2);
  REQUIRE(pi.filesystemCount() == 3);
  REQUIRE(pi.inodeCount() == 50000);
}

TEST_CASE("ProgressInfo done flag") {
  ProgressInfo pi;
  REQUIRE_FALSE(pi.isDone());
  pi.setDone();
  REQUIRE(pi.isDone());
}

TEST_CASE("ProgressInfo formatLine with known total") {
  ProgressInfo pi;
  pi.setFilesystem(1, 2, 1000);
  pi.update(420, 50 * 1024 * 1024);
  std::string line = pi.formatLine(10.0, 42.0, 5.0 * 1024 * 1024);
  // Should contain "Filesystem 1/2:" and "42%" and inode count
  REQUIRE(line.find("Filesystem 1/2:") != std::string::npos);
  REQUIRE(line.find("42%") != std::string::npos);
  REQUIRE(line.find("420") != std::string::npos);
}

TEST_CASE("ProgressInfo formatLine with unknown total") {
  ProgressInfo pi;
  pi.setFilesystem(1, 1, 0);
  pi.update(100, 1024);
  std::string line = pi.formatLine(5.0, 20.0, 1024.0);
  // Should NOT contain "Filesystem" or "%" when inode count is unknown
  REQUIRE(line.find("Filesystem") == std::string::npos);
  REQUIRE(line.find("%") == std::string::npos);
  REQUIRE(line.find("100") != std::string::npos);
}

TEST_CASE("ProgressInfo formatLine with zero filesystem count") {
  ProgressInfo pi;
  pi.setFilesystem(1, 0, 1000);
  pi.update(420, 50 * 1024 * 1024);
  std::string line = pi.formatLine(10.0, 42.0, 5.0 * 1024 * 1024);
  // Should show "Filesystem 1:" without "/0"
  REQUIRE(line.find("Filesystem 1:") != std::string::npos);
  REQUIRE(line.find("Filesystem 1/0:") == std::string::npos);
  REQUIRE(line.find("42%") != std::string::npos);
}
