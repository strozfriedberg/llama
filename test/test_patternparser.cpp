#include <catch2/catch_test_macros.hpp>

#include "patternparser.h"

TEST_CASE("PatternParserInit") {
  PatternParser p;
  PatternDef pDef{LG_KeyOptions{0,0,0}, Encodings{0,0}, "test"};
  REQUIRE_NOTHROW(p.parse(pDef));
}