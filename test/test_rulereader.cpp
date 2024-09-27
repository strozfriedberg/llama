#include <iostream>

#include <catch2/catch_test_macros.hpp>

#include <lightgrep/api.h>

#include <rulereader.h>

TEST_CASE("RuleReader") {
  std::string input(R"(
  rule MyRule {}
  rule MyOtherRule {}
  )");
  std::string input2("rule {}");
  std::string input3(R"(
  rule MyThirdRule {
    file_metadata:
      filesize > 30000
  })");
  RuleReader reader;
  int result = reader.read(input);
  REQUIRE(result == 2);
  result = reader.read(input2);
  REQUIRE(result == -1);
  REQUIRE(reader.getLastError() == "Expected identifier at line 1 column 6");
  result = reader.read(input3);
  REQUIRE(result == 3);
}

LG_HFSM getLgFsmFromRules(const std::vector<Rule>& rules) {
  LG_HPATTERN lgPat = lg_create_pattern();
  LG_HFSM fsm = lg_create_fsm(0, 0);
  LG_Error* err = nullptr;
  uint64_t patternIndex = 0;
  for (const Rule& rule : rules) {
    for (const auto& patternPair : rule.Grep.Patterns.Patterns) {
      for (const PatternDef& pDef : patternPair.second) {
        std::string patNoQuotes = std::string(pDef.Pattern.substr(1, pDef.Pattern.size() - 2));
        lg_parse_pattern(lgPat, patNoQuotes.c_str(), &pDef.Options, &err);
        lg_add_pattern(fsm, lgPat, pDef.Encoding.c_str(), patternIndex, &err);
        ++patternIndex;
      }
    }
  }
  return fsm;
}

TEST_CASE("PopulateLgFSM") {
  std::string input = R"(
  rule MyRule {
    grep:
      patterns:
        s1 = "foobar" fixed encodings=UTF-8
        s2 = "\d{4,8}" encodings=UTF-8,UTF-16LE
      condition:
        all()
  }
  rule AnotherRule {
    grep:
      patterns:
        w1 = "bad-domain.com" fixed nocase
        w2 = "1.1.1.1" fixed encodings=Windows-1252
      condition:
        all()
  })";
  RuleReader reader;
  REQUIRE(reader.read(input) == 2);
  std::vector<Rule> rules = reader.getRules();
  LG_HFSM fsm = getLgFsmFromRules(rules);
  REQUIRE(lg_fsm_pattern_count(fsm) == 5);
  REQUIRE(std::string(lg_fsm_pattern_info(fsm, 0)->Pattern) == "foobar");
  REQUIRE(std::string(lg_fsm_pattern_info(fsm, 0)->EncodingChain) == "UTF-8");
  REQUIRE(lg_fsm_pattern_info(fsm, 0)->UserIndex == 0);
  REQUIRE(std::string(lg_fsm_pattern_info(fsm, 1)->Pattern) == "\\d{4,8}");
  REQUIRE(std::string(lg_fsm_pattern_info(fsm, 1)->EncodingChain) == "UTF-8");
  REQUIRE(lg_fsm_pattern_info(fsm, 1)->UserIndex == 1);
  REQUIRE(std::string(lg_fsm_pattern_info(fsm, 2)->Pattern) == "\\d{4,8}");
  REQUIRE(std::string(lg_fsm_pattern_info(fsm, 2)->EncodingChain) == "UTF-16LE");
  REQUIRE(lg_fsm_pattern_info(fsm, 2)->UserIndex == 2);
  REQUIRE(std::string(lg_fsm_pattern_info(fsm, 3)->Pattern) == "bad-domain.com");
  REQUIRE(std::string(lg_fsm_pattern_info(fsm, 3)->EncodingChain) == "ASCII");
  REQUIRE(lg_fsm_pattern_info(fsm, 3)->UserIndex == 3);
  REQUIRE(std::string(lg_fsm_pattern_info(fsm, 4)->Pattern) == "1.1.1.1");
  REQUIRE(std::string(lg_fsm_pattern_info(fsm, 4)->EncodingChain) == "Windows-1252");
  REQUIRE(lg_fsm_pattern_info(fsm, 4)->UserIndex == 4);
}