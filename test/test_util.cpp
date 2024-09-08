#include <catch2/catch_test_macros.hpp>

#include "util.h"

TEST_CASE("testRandomString") {
  auto s = randomNumString();
  auto t = randomNumString();
  REQUIRE(s != t);
}

