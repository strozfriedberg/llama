#include <catch2/catch_test_macros.hpp>
#include "bitsetvar.h"

TEST_CASE("BitsetVar default-constructs empty", "[bitsetvar]") {
  BitsetVar b;
  REQUIRE(b.size() == 0);
}

TEST_CASE("BitsetVar push_back grows correctly across byte boundaries", "[bitsetvar]") {
  BitsetVar b;
  for (size_t target : {size_t{1}, size_t{7}, size_t{8}, size_t{9}, size_t{17}}) {
    BitsetVar fresh;
    for (size_t i = 0; i < target; ++i) {
      fresh.push_back(i % 2 == 0);
    }
    REQUIRE(fresh.size() == target);
    for (size_t i = 0; i < target; ++i) {
      REQUIRE(fresh.get(i) == (i % 2 == 0));
    }
  }
}

TEST_CASE("BitsetVar set flips bits on and off", "[bitsetvar]") {
  BitsetVar b;
  for (size_t i = 0; i < 20; ++i) b.push_back(false);
  REQUIRE_FALSE(b.get(13));
  b.set(13, true);
  REQUIRE(b.get(13));
  b.set(13, false);
  REQUIRE_FALSE(b.get(13));
}

TEST_CASE("BitsetVar clear resets size and storage", "[bitsetvar]") {
  BitsetVar b;
  for (size_t i = 0; i < 20; ++i) b.push_back(true);
  b.clear();
  REQUIRE(b.size() == 0);
  b.push_back(false);
  REQUIRE(b.size() == 1);
  REQUIRE_FALSE(b.get(0));
}

TEST_CASE("BitsetVar reserve allocates without changing observable state", "[bitsetvar]") {
  BitsetVar b;
  b.reserve(100);
  REQUIRE(b.size() == 0);
  b.push_back(true);
  REQUIRE(b.get(0));
}
