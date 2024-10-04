#include <catch2/catch_test_macros.hpp>

#include "util.h"

TEST_CASE("testIsOdd") {
  REQUIRE(isOdd(5));
  REQUIRE_FALSE(isOdd(2));
}

TEST_CASE("testIsEven") {
  REQUIRE(isEven(4));
  REQUIRE_FALSE(isEven(3));
}