#include <catch2/benchmark/catch_benchmark_all.hpp>
#include <catch2/catch_test_macros.hpp>

#include "yara.h"

namespace {

class YaraLib {
// don't make more than one of these at a time... it would be bad
public:
  YaraLib() { yr_initialize(); }
  ~YaraLib() { yr_finalize(); }
};

std::string SAMPLE_RULE = 
"rule APT_CobaltStrike_Beacon_Indicator {"
"   meta:"
"      description = \"Detects CobaltStrike beacons\""
"      author = \"JPCERT\""
"      reference = \"https://github.com/JPCERTCC/aa-tools/blob/master/cobaltstrikescan.py\""
"      date = \"2018-11-09\""
"      id = \"8508c7a0-0131-59b1-b537-a6d1c6cb2b35\""
"   strings:"
"      $v1 = { 73 70 72 6E 67 00 }"
"      $v2 = { 69 69 69 69 69 69 69 69 }"
"   condition:"
"      uint16(0) == 0x5a4d and filesize < 300KB and all of them"
"}";

  char hex_char(uint8_t val) {
    switch (val) {
    case 0:
      return '0';
    case 1:
      return '1';
    case 2:
      return '2';
    case 3:
      return '3';
    case 4:
      return '4';
    case 5:
      return '5';
    case 6:
      return '6';
    case 7:
      return '7';
    case 8:
      return '8';
    case 9:
      return '9';
    case 10:
      return 'a';
    case 11:
      return 'b';
    case 12:
      return 'c';
    case 13:
      return 'd';
    case 14:
      return 'e';
    case 15:
      return 'f';
    default:
      return 'Z';
    }
  }
} // local namespace

TEST_CASE("testYara") {
  std::unique_ptr<YaraLib> lib(new YaraLib());

  YR_COMPILER* compPtr = nullptr;
  yr_compiler_create(&compPtr);
  std::shared_ptr<YR_COMPILER> comp(compPtr, yr_compiler_destroy);

  REQUIRE(0 == yr_compiler_add_string(comp.get(), SAMPLE_RULE.c_str(), ""));

  YR_RULES* rulesPtr = nullptr;
  REQUIRE(0 == yr_compiler_get_rules(comp.get(), &rulesPtr));
  std::shared_ptr<YR_RULES> rules(rulesPtr, yr_rules_destroy);

  REQUIRE(rulesPtr->num_rules == 1);
  YR_RULE* rule = &((*rulesPtr).rules_table[0]);
  REQUIRE("APT_CobaltStrike_Beacon_Indicator" == std::string(rule->identifier));

  std::vector<std::string> patterns;
  YR_STRING* str = nullptr;
  yr_rule_strings_foreach(rule, str) {
    CAPTURE(str->length);
    CAPTURE(STRING_IS_HEX(str));
    std::string p;
    for (int i = 0; i < str->length; ++i) {
      uint8_t b = str->string[i];
      p += "\\x";
      p += hex_char(b >> 4);
      p += hex_char(b & 0x0f);
    }
    patterns.push_back(p);
  }
}
