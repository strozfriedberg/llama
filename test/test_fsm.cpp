#include <catch2/catch_test_macros.hpp>

#include "lexer.h"
#include "parser.h"
#include "fsm.h"
#include "token.h"


TEST_CASE("initLlamaLgFsm")
{
  PatternParser p;
  PatternDef pDef{LG_KeyOptions{0, 0, 0}, Encodings{0,0}, "test"};
  LgFsmHolder lFsm;
  REQUIRE_NOTHROW(lFsm.addPattern(p.parse(pDef), "ASCII", 0));
  LG_HFSM fsm = lFsm.getFsm();
  REQUIRE(lg_fsm_pattern_count(fsm) == 1);
  REQUIRE(std::string(lg_fsm_pattern_info(fsm, 0)->Pattern) == "test");
  REQUIRE(std::string(lg_fsm_pattern_info(fsm, 0)->EncodingChain) == "ASCII");
  REQUIRE(lg_fsm_pattern_info(fsm, 0)->UserIndex == 0);
}

TEST_CASE("parseWithPatternDef") {
  std::string input = "rule MyRule { grep: patterns: a = \"test\" encodings=UTF-8,UTF-16LE condition: all()}";
  LlamaParser parser(input, LlamaLexer::getTokens(input));
  auto rules = parser.parseRules({0});
  LgFsmHolder lFsm;
  for (const auto& pPair : rules[0].Grep.Patterns.Patterns) {
    lFsm.addPatterns(pPair, parser);
  }
  LG_HFSM fsm = lFsm.getFsm();
  REQUIRE(lFsm.Error() == nullptr);
  REQUIRE(lg_fsm_pattern_count(fsm) == 2);
  REQUIRE(std::string(lg_fsm_pattern_info(fsm, 0)->EncodingChain) == "UTF-8");
  REQUIRE(std::string(lg_fsm_pattern_info(fsm, 0)->Pattern) == "test");
  REQUIRE(std::string(lg_fsm_pattern_info(fsm, 1)->EncodingChain) == "UTF-16LE");
  REQUIRE(std::string(lg_fsm_pattern_info(fsm, 1)->Pattern) == "test");
}