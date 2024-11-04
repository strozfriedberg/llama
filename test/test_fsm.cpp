#include <catch2/catch_test_macros.hpp>

#include "parser.h"
#include "patternparser.h"
#include "fsm.h"

TEST_CASE("initLlamaLgFsm") {
  PatternParser p;
  PatternDef pDef{LG_KeyOptions{0,0,0}, "ASCII", "test"};
  p.parse(pDef);

  LlamaLgFsm fsm;
  REQUIRE_NOTHROW(fsm.addPattern(p.Pat, pDef.Encoding, 0));
}