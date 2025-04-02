#include <catch2/catch_test_macros.hpp>

#include "util.h"
#include "readseek_impl.h"

TEST_CASE("testRandomString") {
  auto s = randomNumString();
  auto t = randomNumString();
  REQUIRE(s != t);
}

TEST_CASE("testIsOdd") {
  REQUIRE(isOdd(5));
  REQUIRE_FALSE(isOdd(2));
}

TEST_CASE("testIsEven") {
  REQUIRE(isEven(4));
  REQUIRE_FALSE(isEven(3));
}

TEST_CASE("testIsPDF") {
  std::string testPath = "test/data/small.pdf";
  std::shared_ptr<FILE> f(fopen(testPath.c_str(), "r"), std::fclose);
  ReadSeekFile rs(f);
  REQUIRE(isPDF(rs));
  REQUIRE(rs.tellg() == 0);
}
