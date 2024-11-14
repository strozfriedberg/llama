#include <iostream>

#include <catch2/catch_test_macros.hpp>

#include <lightgrep/api.h>

#include "fsm.h"
#include "patternparser.h"
#include "rulereader.h"

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
  REQUIRE(result == true);
  result = reader.read(input2);
  auto pErrors = reader.getParser().errors();
  REQUIRE(pErrors.size() == 2);
  REQUIRE(std::string(pErrors[0].what()) == "Expected identifier at line 1 column 6");
  REQUIRE(result == false);
  REQUIRE(reader.getLastError() == "");
  result = reader.read(input3);
  REQUIRE(result == true);
}

LgFsmHolder getLgFsmFromRules(const std::vector<Rule>& rules, const RuleReader& reader) {
  PatternParser patParser;
  LgFsmHolder fsm;
  uint64_t patternIndex = 0;
  for (const Rule& rule : rules) {
    for (const auto& patternPair : rule.Grep.Patterns.Patterns) {
      auto pDef = patternPair.second;
      if (pDef.Enc.first == pDef.Enc.second) {
        // No encodings were defined for the pattern, so parse with ASCII only
        fsm.addPattern(patParser.parse(pDef), "ASCII", patternIndex);
        ++patternIndex;
      }
      else {
        for (uint64_t i = pDef.Enc.first; i < pDef.Enc.second; i += 2) {
          fsm.addPattern(patParser.parse(pDef), std::string(reader.getParser().Tokens[i].Lexeme).c_str(), patternIndex);
          ++patternIndex;
        }
      }
    }
  }
  return fsm;
}

std::string getPat(LG_HFSM fsm, unsigned int i) {
  return std::string(lg_fsm_pattern_info(fsm, i)->Pattern);
}

std::string getEnc(LG_HFSM fsm, unsigned int i) {
  return std::string(lg_fsm_pattern_info(fsm, i)->EncodingChain);
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
  REQUIRE(reader.read(input) == true);
  std::vector<Rule> rules = reader.getRules();
  LgFsmHolder lFsm = getLgFsmFromRules(rules, reader);
  LG_HFSM fsm = lFsm.getFsm();
  REQUIRE(lg_fsm_pattern_count(fsm) == 5);
  bool a = false, b = false, c = false, d = false, e = false;
  for (unsigned int i = 0; i < lg_fsm_pattern_count(fsm); ++i) {
    REQUIRE(lg_fsm_pattern_info(fsm, i)->UserIndex == i);
    a = (getPat(fsm, i) == "foobar" && getEnc(fsm, i) == "UTF-8") || a;
    b = (getPat(fsm, i) == "\\d{4,8}" && getEnc(fsm, i) == "UTF-8") || b;
    c = (getPat(fsm, i) == "\\d{4,8}" && getEnc(fsm, i) == "UTF-16LE") || c;
    d = (getPat(fsm, i) == "bad-domain.com" && getEnc(fsm, i) == "ASCII") || d;
    e = (getPat(fsm, i) == "1.1.1.1" && getEnc(fsm, i) == "Windows-1252") || e;
  }
  REQUIRE(a);
  REQUIRE(b);
  REQUIRE(c);
  REQUIRE(d);
  REQUIRE(e);
}