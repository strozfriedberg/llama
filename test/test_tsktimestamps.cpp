#include <catch2/catch_test_macros.hpp>

#include "timestamps.h"
#include "tsktimestamps.h"

TEST_CASE("testTskConvertTimestamps") {
  TSK_FS_META meta;
  std::memset(&meta, 0, sizeof(meta));

  meta.atime = 1578364822; // 2020-01-07 02:40:22
  meta.atime_nano = 123456700;
  meta.crtime = 31337; // 1970-01-01 08:42:17
  meta.crtime_nano = 123456400;
  meta.ctime = 234123870; // 1977-06-02 18:24:30
  meta.ctime_nano = 315227845;
  meta.mtime = 314159265; // 1979-12-16 02:27:45
  meta.mtime_nano = 999999999;

  CommonTimestampGetter ts;

  // basic four are good
  REQUIRE("2020-01-07 02:40:22.1234567" == ts.accessed(meta).as<std::string>());
  REQUIRE("1970-01-01 08:42:17.1234564" == ts.created(meta).as<std::string>());
  REQUIRE("1977-06-02 18:24:30.315227845" == ts.metadata(meta).as<std::string>());
  REQUIRE("1979-12-16 02:27:45.999999999" == ts.modified(meta).as<std::string>());
}

TEST_CASE("testTskConvertEpochBeginningIsNull") {
  TSK_FS_META meta;
  std::memset(&meta, 0, sizeof(meta));

  meta.atime = 0; // 1970-01-01 00:00:00
  meta.atime_nano = 0;
  meta.crtime = 1; // 1970-01-01 00:00::01
  meta.crtime_nano = 0;
  meta.ctime = 0; // 1970-01-01 00:00:00.000000001
  meta.ctime_nano = 1;
  meta.mtime = 0; // 1970-01-01 00:00:00
  meta.mtime_nano = 0;

  CommonTimestampGetter ts;

  // basic four are good
  REQUIRE(ts.accessed(meta).is_null());
  REQUIRE("1970-01-01 00:00:01" == ts.created(meta).as<std::string>());
  REQUIRE("1970-01-01 00:00:00.000000001" == ts.metadata(meta).as<std::string>());
  REQUIRE(ts.modified(meta).is_null());
}

TEST_CASE("testTskConvertNTFSTimestamps") {
  TSK_FS_META meta;
  std::memset(&meta, 0, sizeof(meta));

  meta.atime = 1578364822; // 2020-01-07 02:40:22
  meta.atime_nano = 123456700;
  meta.crtime = 31337; // 1970-01-01 08:42:17
  meta.crtime_nano = 123456400;
  meta.ctime = 234123870; // 1977-06-02 18:24:30
  meta.ctime_nano = 315227845;
  meta.mtime = 314159265; // 1979-12-16 02:27:45
  meta.mtime_nano = 999999999;

  meta.time2.ntfs.fn_atime = 1578602384; // 2020-01-09 20:39:44
  meta.time2.ntfs.fn_atime_nano = 1000;
  meta.time2.ntfs.fn_crtime = 1425542407; // 2015-03-05 08:00:07
  meta.time2.ntfs.fn_crtime_nano = 0;
  meta.time2.ntfs.fn_ctime = 1371346223; // 2013-06-16 01:30:23
  meta.time2.ntfs.fn_ctime_nano = 900000000;
  meta.time2.ntfs.fn_mtime = 1123946859; // 2005-08-13 15:27:39
  meta.time2.ntfs.fn_mtime_nano = 1001001;

  NTFSTimestampGetter ts;

  // basic four are good
  REQUIRE("2020-01-07 02:40:22.1234567" == ts.accessed(meta).as<std::string>());
  REQUIRE("1970-01-01 08:42:17.1234564" == ts.created(meta).as<std::string>());
  REQUIRE("1977-06-02 18:24:30.315227845" == ts.metadata(meta).as<std::string>());
  REQUIRE("1979-12-16 02:27:45.999999999" == ts.modified(meta).as<std::string>());
  // and filename attr timestamps
  REQUIRE("2020-01-09 20:39:44.000001" == ts.fn_accessed(meta).as<std::string>());
  REQUIRE("2015-03-05 08:00:07" == ts.fn_created(meta).as<std::string>());
  REQUIRE("2013-06-16 01:30:23.9" == ts.fn_metadata(meta).as<std::string>());
  REQUIRE("2005-08-13 15:27:39.001001001" == ts.fn_modified(meta).as<std::string>());
  // not these
  REQUIRE(ts.deleted(meta).is_null());
  REQUIRE(ts.backup(meta).is_null());
}

TEST_CASE("testTskConvertMacTimestamps") {
  TSK_FS_META meta;
  std::memset(&meta, 0, sizeof(meta));

  meta.atime = 1578364822; // 2020-01-07 02:40:22
  meta.atime_nano = 123456700;
  meta.crtime = 31337; // 1970-01-01 08:42:17
  meta.crtime_nano = 123456400;
  meta.ctime = 234123870; // 1977-06-02 18:24:30
  meta.ctime_nano = 315227845;
  meta.mtime = 314159265; // 1979-12-16 02:27:45
  meta.mtime_nano = 999999999;

  meta.time2.hfs.bkup_time = 1578602384; // 2020-01-09 20:39:44
  meta.time2.hfs.bkup_time_nano = 1000;

  HFSTimestampGetter ts;

  // basic four are good
  REQUIRE("2020-01-07 02:40:22.1234567" == ts.accessed(meta).as<std::string>());
  REQUIRE("1970-01-01 08:42:17.1234564" == ts.created(meta).as<std::string>());
  REQUIRE("1977-06-02 18:24:30.315227845" == ts.metadata(meta).as<std::string>());
  REQUIRE("1979-12-16 02:27:45.999999999" == ts.modified(meta).as<std::string>());
  // and hfs+ backup
  REQUIRE("2020-01-09 20:39:44.000001" == ts.backup(meta).as<std::string>());
  // but not these
  REQUIRE(ts.deleted(meta).is_null());
  REQUIRE(ts.fn_accessed(meta).is_null());
  REQUIRE(ts.fn_created(meta).is_null());
  REQUIRE(ts.fn_metadata(meta).is_null());
  REQUIRE(ts.fn_modified(meta).is_null());
}

TEST_CASE("testTskConvertLinuxTimestamps") {
  TSK_FS_META meta;
  std::memset(&meta, 0, sizeof(meta));

  meta.atime = 1578364822; // 2020-01-07 02:40:22
  meta.atime_nano = 123456700;
  meta.crtime = 31337; // 1970-01-01 08:42:17
  meta.crtime_nano = 123456400;
  meta.ctime = 234123870; // 1977-06-02 18:24:30
  meta.ctime_nano = 315227845;
  meta.mtime = 314159265; // 1979-12-16 02:27:45
  meta.mtime_nano = 999999999;

  meta.time2.hfs.bkup_time = 1578602384; // 2020-01-09 20:39:44
  meta.time2.hfs.bkup_time_nano = 1000;

  EXTTimestampGetter ts;

  // basic four are good
  REQUIRE("2020-01-07 02:40:22.1234567" == ts.accessed(meta).as<std::string>());
  REQUIRE("1970-01-01 08:42:17.1234564" == ts.created(meta).as<std::string>());
  REQUIRE("1977-06-02 18:24:30.315227845" == ts.metadata(meta).as<std::string>());
  REQUIRE("1979-12-16 02:27:45.999999999" == ts.modified(meta).as<std::string>());
  // and deleted
  REQUIRE("2020-01-09 20:39:44.000001" == ts.deleted(meta).as<std::string>());
  // but not these
  REQUIRE(ts.backup(meta).is_null());
  REQUIRE(ts.fn_accessed(meta).is_null());
  REQUIRE(ts.fn_created(meta).is_null());
  REQUIRE(ts.fn_metadata(meta).is_null());
  REQUIRE(ts.fn_modified(meta).is_null());
}

TEST_CASE("formatTimestamp - zero handling") {
  // Both zero = empty string
  REQUIRE("" == formatTimestamp(0, 0));

  // Only unix_time zero with ns > 0 = epoch with fractional
  REQUIRE("1970-01-01 00:00:00.000000001" == formatTimestamp(0, 1));
  REQUIRE("1970-01-01 00:00:00.1" == formatTimestamp(0, 100000000));

  // Only ns zero = normal timestamp
  REQUIRE("1970-01-01 00:00:01" == formatTimestamp(1, 0));
}

TEST_CASE("formatTimestamp - negative timestamps") {
  // One second before epoch
  REQUIRE("1969-12-31 23:59:59" == formatTimestamp(-1, 0));

  // NTFS epoch (1601-01-01 00:00:00 = -11644473600)
  REQUIRE("1601-01-01 00:00:00" == formatTimestamp(-11644473600, 0));

  // Year 1900 (common in old filesystems)
  REQUIRE("1900-01-01 00:00:00" == formatTimestamp(-2208988800, 0));

  // Negative with fractional seconds
  REQUIRE("1969-12-31 23:59:59.5" == formatTimestamp(-1, 500000000));
}

TEST_CASE("formatTimestamp - year boundaries") {
  // 1969 → 1970 transition
  REQUIRE("1969-12-31 23:59:59" == formatTimestamp(-1, 0));
  REQUIRE("1970-01-01 00:00:00.000000001" == formatTimestamp(0, 1));
  REQUIRE("1970-01-01 00:00:01" == formatTimestamp(1, 0));

  // Y2K transition
  REQUIRE("1999-12-31 23:59:59" == formatTimestamp(946684799, 0));
  REQUIRE("2000-01-01 00:00:00" == formatTimestamp(946684800, 0));

  // 32-bit overflow (2038-01-19 03:14:07)
  REQUIRE("2038-01-19 03:14:07" == formatTimestamp(2147483647, 0));
  REQUIRE("2038-01-19 03:14:08" == formatTimestamp(2147483648, 0));
}

TEST_CASE("formatTimestamp - fractional seconds") {
  // Trailing zero trimming
  REQUIRE("2020-01-01 00:00:00.1" == formatTimestamp(1577836800, 100000000));
  REQUIRE("2020-01-01 00:00:00.12" == formatTimestamp(1577836800, 120000000));
  REQUIRE("2020-01-01 00:00:00.123" == formatTimestamp(1577836800, 123000000));
  REQUIRE("2020-01-01 00:00:00.1234" == formatTimestamp(1577836800, 123400000));
  REQUIRE("2020-01-01 00:00:00.12345" == formatTimestamp(1577836800, 123450000));
  REQUIRE("2020-01-01 00:00:00.123456" == formatTimestamp(1577836800, 123456000));
  REQUIRE("2020-01-01 00:00:00.1234567" == formatTimestamp(1577836800, 123456700));
  REQUIRE("2020-01-01 00:00:00.12345678" == formatTimestamp(1577836800, 123456780));
  REQUIRE("2020-01-01 00:00:00.123456789" == formatTimestamp(1577836800, 123456789));

  // Maximum valid nanoseconds (999999999)
  REQUIRE("2020-01-01 00:00:00.999999999" == formatTimestamp(1577836800, 999999999));

  // Invalid nanoseconds >= 1000000000 = ignore fractional
  REQUIRE("2020-01-01 00:00:00" == formatTimestamp(1577836800, 1000000000));
  REQUIRE("2020-01-01 00:00:00" == formatTimestamp(1577836800, 9999999999));

  // Single nanosecond
  REQUIRE("2020-01-01 00:00:00.000000001" == formatTimestamp(1577836800, 1));
}

TEST_CASE("formatTimestamp - leap years") {
  // 2000 is a leap year (divisible by 400)
  REQUIRE("2000-02-29 12:00:00" == formatTimestamp(951825600, 0));

  // 2020 is a leap year
  REQUIRE("2020-02-29 12:00:00" == formatTimestamp(1582977600, 0));

  // 1900 is NOT a leap year (divisible by 100 but not 400)
  // Feb 28, 1900 = -2203977600, Mar 1, 1900 = -2203891200
  REQUIRE("1900-02-28 12:00:00" == formatTimestamp(-2203934400, 0));
  REQUIRE("1900-03-01 12:00:00" == formatTimestamp(-2203848000, 0));

  // 2024 is a leap year
  REQUIRE("2024-02-29 12:00:00" == formatTimestamp(1709208000, 0));
}
