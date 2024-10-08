#include "timestamps.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <boost/date_time/posix_time/posix_time.hpp>
#pragma GCC diagnostic pop

// TODO: we know the precision, so we should print fractional seconds that way
// TODO: add direct tests of this
// TODO: use std::to_chars, get rid of std::ostringstream?

namespace {
  static const auto UNIX_BIRTHDAY = boost::gregorian::date(1970, 1, 1);
  static const boost::posix_time::ptime START_TIME(UNIX_BIRTHDAY);
}

std::string formatTimestamp(int64_t unix_time, uint32_t ns, std::ostringstream& buf) {
  std::string ret;
  ret.reserve(30);

  if (0 == unix_time) {
    if (0 == ns) {
      return "";
    }
    // some C libs will otherwise print an empty string with strftime
    ret.append("1970-01-01 00:00:00");
  }
  else {
    // Boost, you tried.
    //const boost::posix_time::ptime pt = boost::posix_time::from_time_t(unix_time);
    try {
      const int64_t min = unix_time / 60;
      // Boost Date-Time has a maximum year of around 10000,
      // Which means the number of _hours_ since 1970 will fit in a int32_t
      // (In some versions of Boost (1_64), the time_duration takes an int32_t hours param,
      //  While in others it takes a std::time_t hours)
      boost::posix_time::time_duration duration(
        static_cast<boost::posix_time::time_res_traits::hour_type>(min / 60),
        static_cast<boost::posix_time::time_res_traits::min_type>(min % 60),
        static_cast<boost::posix_time::time_res_traits::sec_type>(unix_time % 60)
      );
      const boost::posix_time::ptime pt = START_TIME + duration;
      const tm pt_tm = boost::posix_time::to_tm(pt);
      char tbuf[100];
      const size_t len = strftime(tbuf, 100, "%Y-%m-%d %H:%M:%S", &pt_tm);
      if (len) {
        ret.append(tbuf);
      } // else error??? [a greybeard once said, do not test for errors you can't handle... -- jls]
    }
    catch (const std::exception&) {
      // FIXME: Under what conditions can this happen? Should we clear ret
      // if it does?
    }
  }

  // fractional seconds
  if (0 < ns && ns < 1000000000) {
    buf << double(ns) / 1000000000;
    auto frac = buf.str();
    ret.append(frac.substr(1, frac.find_last_not_of('0'))); // no leading or trailing zeroes
    buf.str("");
  }
  return ret;
}

