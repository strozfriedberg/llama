#include <catch2/catch_test_macros.hpp>

#include "parser.h"
#include "patternparser.h"
#include "fsm.h"

TEST_CASE("initLlamaLgFsm") {
  PatternParser p;
  PatternDef pDef{LG_KeyOptions{0,0,0}, "ASCII", "test"};
  LlamaLgFsm fsm;
  REQUIRE_NOTHROW(fsm.addPattern(p.parse(pDef), pDef.Encoding, 0));
}