// ABOUTME: Benchmarks for formatTimestamp() function
// ABOUTME: Measures performance of timestamp formatting with various scenarios

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include "timestamps.h"

#include <random>
#include <vector>
#include <sstream>

namespace {
  // Generate test timestamps within the past year
  struct TimestampData {
    std::vector<int64_t> unix_times;
    std::vector<uint32_t> nanoseconds;

    TimestampData(size_t count = 1000) {
      unix_times.reserve(count);
      nanoseconds.reserve(count);

      std::mt19937_64 gen(42); // Fixed seed for reproducibility

      // Base: approximately now (Feb 2026)
      const int64_t base_time = 1738540800; // 2025-02-03 00:00:00 UTC
      const int64_t year_in_seconds = 365LL * 24 * 60 * 60;

      std::uniform_int_distribution<int64_t> time_dist(0, year_in_seconds);
      std::uniform_int_distribution<uint32_t> ns_dist(0, 999999999);

      for (size_t i = 0; i < count; ++i) {
        unix_times.push_back(base_time + time_dist(gen));
        nanoseconds.push_back(ns_dist(gen));
      }
    }
  };

  // Singleton to avoid regenerating test data
  const TimestampData& getTestData() {
    static TimestampData data;
    return data;
  }
}

TEST_CASE("formatTimestamp benchmarks") {
  const auto& data = getTestData();
  std::ostringstream buf;

  BENCHMARK("formatTimestamp - with nanoseconds (1000 timestamps)") {
    std::string result;
    for (size_t i = 0; i < data.unix_times.size(); ++i) {
      result = formatTimestamp(data.unix_times[i], data.nanoseconds[i], buf);
    }
    return result;
  };

  BENCHMARK("formatTimestamp - without nanoseconds (1000 timestamps)") {
    std::string result;
    for (size_t i = 0; i < data.unix_times.size(); ++i) {
      result = formatTimestamp(data.unix_times[i], 0, buf);
    }
    return result;
  };
}
