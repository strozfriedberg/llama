// ABOUTME: Formats Unix timestamps as human-readable "YYYY-MM-DD HH:MM:SS[.nnnnnnnnn]" strings
// ABOUTME: Uses C++20 chrono for date decomposition and fixed-point integer arithmetic for fractional seconds

#include "timestamps.h"

#include <chrono>
#include <format>

namespace {
  // Lookup table for two-digit ASCII conversion (00-99)
  static const char digits[100][2] = {
    {'0','0'}, {'0','1'}, {'0','2'}, {'0','3'}, {'0','4'}, {'0','5'}, {'0','6'}, {'0','7'}, {'0','8'}, {'0','9'},
    {'1','0'}, {'1','1'}, {'1','2'}, {'1','3'}, {'1','4'}, {'1','5'}, {'1','6'}, {'1','7'}, {'1','8'}, {'1','9'},
    {'2','0'}, {'2','1'}, {'2','2'}, {'2','3'}, {'2','4'}, {'2','5'}, {'2','6'}, {'2','7'}, {'2','8'}, {'2','9'},
    {'3','0'}, {'3','1'}, {'3','2'}, {'3','3'}, {'3','4'}, {'3','5'}, {'3','6'}, {'3','7'}, {'3','8'}, {'3','9'},
    {'4','0'}, {'4','1'}, {'4','2'}, {'4','3'}, {'4','4'}, {'4','5'}, {'4','6'}, {'4','7'}, {'4','8'}, {'4','9'},
    {'5','0'}, {'5','1'}, {'5','2'}, {'5','3'}, {'5','4'}, {'5','5'}, {'5','6'}, {'5','7'}, {'5','8'}, {'5','9'},
    {'6','0'}, {'6','1'}, {'6','2'}, {'6','3'}, {'6','4'}, {'6','5'}, {'6','6'}, {'6','7'}, {'6','8'}, {'6','9'},
    {'7','0'}, {'7','1'}, {'7','2'}, {'7','3'}, {'7','4'}, {'7','5'}, {'7','6'}, {'7','7'}, {'7','8'}, {'7','9'},
    {'8','0'}, {'8','1'}, {'8','2'}, {'8','3'}, {'8','4'}, {'8','5'}, {'8','6'}, {'8','7'}, {'8','8'}, {'8','9'},
    {'9','0'}, {'9','1'}, {'9','2'}, {'9','3'}, {'9','4'}, {'9','5'}, {'9','6'}, {'9','7'}, {'9','8'}, {'9','9'}
  };
}

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

  // Year: split into two 2-digit pairs
  const unsigned year_high = year / 100;
  const unsigned year_low = year % 100;
  buf[0] = digits[year_high][0];
  buf[1] = digits[year_high][1];
  buf[2] = digits[year_low][0];
  buf[3] = digits[year_low][1];
  buf[4] = '-';

  // Month, day, hour, min, sec: direct 2-digit lookups
  buf[5] = digits[month][0];
  buf[6] = digits[month][1];
  buf[7] = '-';
  buf[8] = digits[day][0];
  buf[9] = digits[day][1];
  buf[10] = ' ';
  buf[11] = digits[hour][0];
  buf[12] = digits[hour][1];
  buf[13] = ':';
  buf[14] = digits[min][0];
  buf[15] = digits[min][1];
  buf[16] = ':';
  buf[17] = digits[sec][0];
  buf[18] = digits[sec][1];

  int num = 19;

  // Fractional seconds: fixed-point integer arithmetic, no floating point
  if (ns > 0 && ns < 1000000000) {
    buf[19] = '.';
    num = 29;
    uint32_t val = ns;

    // Extract 9 digits: 1 single digit (least significant) + 4 pairs
    buf[28] = '0' + val % 10;
    val /= 10;
    // Extract 4 pairs of digits (buf[20..27])
    for (int i = 26; i >= 20; i -= 2) {
      const unsigned pair = val % 100;
      buf[i + 1] = digits[pair][1];
      buf[i] = digits[pair][0];
      val /= 100;
    }

    // Trim trailing zeros
    while (num > 20 && buf[num - 1] == '0') {
      --num;
    }
  }
  out.assign(buf, num);
}

std::string formatTimestamp(int64_t unix_time, uint64_t ns) {
  std::string result;
  formatTimestamp(unix_time, ns, result);
  return result;
}

std::string nowISO() {
  auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
  return std::format("{:%FT%T}", now);
}
