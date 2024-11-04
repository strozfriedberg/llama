#include <catch2/catch_test_macros.hpp>

#include "parser.h"
#include "patternparser.h"
#include "fsm.h"

TEST_CASE("initLlamaLgFsm")
{
  PatternParser p;
  PatternDef pDef{LG_KeyOptions{0, 0, 0}, "ASCII", "test"};
  LgFsmHolder lFsm;
  REQUIRE_NOTHROW(lFsm.addPattern(p.parse(pDef), pDef.Encoding, 0));
  LG_HFSM fsm = lFsm.getFsm();
  REQUIRE(lg_fsm_pattern_count(fsm) == 1);
  REQUIRE(std::string(lg_fsm_pattern_info(fsm, 0)->Pattern) == "test");
  REQUIRE(std::string(lg_fsm_pattern_info(fsm, 0)->EncodingChain) == "ASCII");
  REQUIRE(lg_fsm_pattern_info(fsm, 0)->UserIndex == 0);
}