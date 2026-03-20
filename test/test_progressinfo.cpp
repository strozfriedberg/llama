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
  pi.setFilesystem(2, 3, 50000, 1024ULL * 1024 * 1024 * 100);
  REQUIRE(pi.inodesProcessed() == 0);
  REQUIRE(pi.bytesProcessed() == 0);
  REQUIRE(pi.filesystemIndex() == 2);
  REQUIRE(pi.filesystemCount() == 3);
  REQUIRE(pi.inodeCount() == 50000);
  REQUIRE(pi.totalBytes() == 1024ULL * 1024 * 1024 * 100);
}

TEST_CASE("ProgressInfo done flag") {
  ProgressInfo pi;
  REQUIRE_FALSE(pi.isDone());
  pi.setDone();
  REQUIRE(pi.isDone());
}

TEST_CASE("ProgressInfo formatLine with known total") {
  ProgressInfo pi;
  pi.setFilesystem(1, 2, 1000, 500ULL * 1024 * 1024);
  pi.update(420, 50ULL * 1024 * 1024);
  std::string line = pi.formatLine(10.0);
  // Should contain filesystem header with percentage, inode counts, and byte counts
  REQUIRE(line.find("Filesystem 1/2:") != std::string::npos);
  REQUIRE(line.find("42%") != std::string::npos);
  REQUIRE(line.find("420") != std::string::npos);
  REQUIRE(line.find("1,000") != std::string::npos);
  // Rates are cumulative: 420 inodes / 10s = 42 files/s, 50MB / 10s = 5 MB/s
  REQUIRE(line.find("42 files/s") != std::string::npos);
  REQUIRE(line.find("5.0 MB/s") != std::string::npos);
  // Bytes processed/total
  REQUIRE(line.find("50.0 MB") != std::string::npos);
  REQUIRE(line.find("500.0 MB") != std::string::npos);
}

TEST_CASE("ProgressInfo formatLine with unknown total") {
  ProgressInfo pi;
  pi.setFilesystem(1, 1, 0, 0);
  pi.update(100, 1024);
  std::string line = pi.formatLine(5.0);
  // Should NOT contain percentage when inode count is unknown
  REQUIRE(line.find("%") == std::string::npos);
  REQUIRE(line.find("100") != std::string::npos);
}

TEST_CASE("ProgressInfo formatLine with zero filesystem count") {
  ProgressInfo pi;
  pi.setFilesystem(1, 0, 1000, 500ULL * 1024 * 1024);
  pi.update(420, 50ULL * 1024 * 1024);
  std::string line = pi.formatLine(10.0);
  // Should show "Filesystem 1:" without "/0"
  REQUIRE(line.find("Filesystem 1:") != std::string::npos);
  REQUIRE(line.find("Filesystem 1/0:") == std::string::npos);
  REQUIRE(line.find("42%") != std::string::npos);
}

TEST_CASE("ProgressInfo formatLine uses cumulative rates") {
  ProgressInfo pi;
  pi.setFilesystem(1, 1, 100000, 10ULL * 1024 * 1024 * 1024);
  pi.update(5000, 500ULL * 1024 * 1024);
  std::string line = pi.formatLine(10.0);
  // 5000/10 = 500 files/s, 500MB/10 = 50 MB/s
  REQUIRE(line.find("500 files/s") != std::string::npos);
  REQUIRE(line.find("50.0 MB/s") != std::string::npos);
}

TEST_CASE("ProgressInfo formatLine shows zero rates at zero elapsed") {
  ProgressInfo pi;
  pi.setFilesystem(1, 1, 1000, 1024ULL * 1024);
  pi.update(100, 1024);
  std::string line = pi.formatLine(0.0);
  REQUIRE(line.find("0 files/s") != std::string::npos);
}
