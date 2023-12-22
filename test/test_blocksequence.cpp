#include <catch2/catch_test_macros.hpp>

#include "blocksequence_impl.h"

TEST_CASE("sizes") {
  REQUIRE(sizeof(FileBlockSequence) < 16384);
  REQUIRE(sizeof(TskBlockSequence) < 16384);
}
