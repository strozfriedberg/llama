#include <catch2/catch_test_macros.hpp>

#include "readseek_impl.h"

void basicReadSeekTest(const std::vector<uint8_t>& srcbuf, ReadSeek& rs) {
  std::vector<uint8_t> buf;

  REQUIRE(rs.tellg() == 0);
  REQUIRE(rs.size() == srcbuf.size());
  REQUIRE(size_t(rs.read(srcbuf.size(), buf)) == srcbuf.size());
  REQUIRE(buf == srcbuf);
  REQUIRE(rs.tellg() == rs.size());
  REQUIRE(rs.read(1, buf) == 0); // read past end should return 0
  REQUIRE(buf == srcbuf); // unsuccessful read should not modify the input buffer

  REQUIRE(rs.seek(1) == 1);
  REQUIRE(rs.tellg() == 1);
  REQUIRE(rs.read(2, buf) == 2);
  REQUIRE(buf == std::vector<uint8_t>{32, 113});
  REQUIRE(rs.tellg() == 3);

  REQUIRE(rs.seek(rs.size() + 1) == rs.size());
}

TEST_CASE("readSeekBuf") {
  std::vector<uint8_t> srcbuf{ 35, 32, 113, 65 };

  ReadSeekBuf rs(srcbuf);
  basicReadSeekTest(srcbuf, rs);
}
