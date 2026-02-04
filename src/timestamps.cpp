// ABOUTME: Formats Unix timestamps as human-readable "YYYY-MM-DD HH:MM:SS[.nnnnnnnnn]" strings
// ABOUTME: Uses C++20 chrono for date decomposition and fixed-point integer arithmetic for fractional seconds

#include "timestamps.h"

#include <chrono>

void formatTimestamp(int64_t unix_time, uint64_t ns, std::string& out) {
  out.clear();

  // (0, 0) is the null timestamp sentinel: "timestamp not set"
  if (unix_time == 0 && ns == 0) {
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
  char buf[19];
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
  out.append(buf, 19);

  // Fractional seconds: fixed-point integer arithmetic, no floating point
  if (ns > 0 && ns < 1000000000) {
    // Decompose ns into 9 decimal digits
    char frac[9];
    uint32_t val = ns;
    for (int i = 8; i >= 0; --i) {
      frac[i] = '0' + val % 10;
      val /= 10;
    }

    // Trim trailing zeros
    int last = 8;
    while (last > 0 && frac[last] == '0') {
      --last;
    }

    out.push_back('.');
    out.append(frac, last + 1);
  }
}

std::string formatTimestamp(int64_t unix_time, uint64_t ns) {
  std::string result;
  formatTimestamp(unix_time, ns, result);
  return result;
}
