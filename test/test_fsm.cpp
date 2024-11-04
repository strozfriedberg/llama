#include <catch2/catch_test_macros.hpp>

#include "parser.h"
#include "patternparser.h"
#include "fsm.h"

TEST_CASE("initLlamaLgFsm") {
  PatternParser p;
  PatternDef pDef{LG_KeyOptions{0,0,0}, "ASCII", "test"};
  LlamaLgFsm fsm;
  REQUIRE_NOTHROW(fsm.addPattern(p.parse(pDef), pDef.Encoding, 0));
  REQUIRE(lg_fsm_pattern_count(fsm.Fsm) == 1);
  REQUIRE(std::string(lg_fsm_pattern_info(fsm.Fsm, 0)->Pattern) == "test");
  REQUIRE(std::string(lg_fsm_pattern_info(fsm.Fsm, 0)->EncodingChain) == "ASCII");
  REQUIRE(lg_fsm_pattern_info(fsm.Fsm, 0)->UserIndex == 0);
}