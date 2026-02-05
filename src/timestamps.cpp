// ABOUTME: Formats Unix timestamps as human-readable "YYYY-MM-DD HH:MM:SS[.nnnnnnnnn]" strings
// ABOUTME: Uses C++20 chrono for date decomposition and fixed-point integer arithmetic for fractional seconds

#include "timestamps.h"

#include <chrono>

void formatTimestamp(int64_t unix_time, uint64_t ns, std::string& out) {
  // (0, 0) is the null timestamp sentinel: "timestamp not set"
  if (unix_time == 0 && ns == 0) {
    out.clear();
    return;
  }

  out.reserve(30); // max: "YYYY-MM-DD HH:MM:SS.nnnnnnnnn"

  // Decompose into calendar date and time-of-day
  const auto sys_time = std::chrono::sys_seconds{std::chrono::seconds{unix_time}};
  const auto dp = std::chrono::floor<std::chrono::days>(sys_time);
  const auto ymd = std::chrono::year_month_day{dp};
  const auto tod = std::chrono::hh_mm_ss<std::chrono::seconds>{sys_time - dp};

  const int year   = static_cast<int>(ymd.year());
  const unsigned month = static_cast<unsigned>(ymd.month());
  const unsigned day   = static_cast<unsigned>(ymd.day());
  const unsigned hour  = tod.hours().count();
  const unsigned min   = tod.minutes().count();
  const unsigned sec   = tod.seconds().count();


  // Format "YYYY-MM-DD HH:MM:SS" directly into a stack buffer
  char buf[29];
  buf[ 0] = '0' + year / 1000;
  buf[ 1] = '0' + (year / 100) % 10;
  buf[ 2] = '0' + (year / 10) % 10;
  buf[ 3] = '0' + year % 10;
  buf[ 4] = '-';
  buf[ 5] = '0' + month / 10;
  buf[ 6] = '0' + month % 10;
  buf[ 7] = '-';
  buf[ 8] = '0' + day / 10;
  buf[ 9] = '0' + day % 10;
  buf[10] = ' ';
  buf[11] = '0' + hour / 10;
  buf[12] = '0' + hour % 10;
  buf[13] = ':';
  buf[14] = '0' + min / 10;
  buf[15] = '0' + min % 10;
  buf[16] = ':';
  buf[17] = '0' + sec / 10;
  buf[18] = '0' + sec % 10;

  int num = 19;

  // Fractional seconds: fixed-point integer arithmetic, no floating point
  if (ns > 0 && ns < 1000000000) {
    // Decompose ns into 9 decimal digits
    buf[19] = '.';
    num = 29;
    uint32_t val = ns;
    for (int i = 28; i >= 20; --i) {
      buf[i] = '0' + val % 10;
      val /= 10;
    }
    while (num > 20 && buf[num - 1] == '0') {
      --num; // trim trailing zeros
    }
  }
  out.assign(buf, num);
}

std::string formatTimestamp(int64_t unix_time, uint64_t ns) {
  std::string result;
  formatTimestamp(unix_time, ns, result);
  return result;
}
