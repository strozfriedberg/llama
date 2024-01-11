#include <catch2/benchmark/catch_benchmark_all.hpp>
#include <catch2/catch_test_macros.hpp>

#include "yara.h"

namespace {

  std::string holmes2(
R"("My dear fellow," said Sherlock Holmes as we sat on either side of the fire in his lodgings at Baker Street, "life is infinitely stranger than anything which the mind of man could invent. We would not dare to conceive the things which are really mere commonplaces of existence. If we could fly out of that window hand in hand, hover over this great city, gently remove the roofs, and peep in at the queer things which are going on, the strange coincidences, the plannings, the cross-purposes, the wonderful chains of events, working through generations, and leading to the most outre results, it would make all fiction with its conventionalities and foreseen conclusions most stale and unprofitable."

"And yet I am not convinced of it," I answered. "The cases which come to light in the papers are, as a rule, bald enough, and vulgar enough. We have in our police reports realism pushed to its extreme limits, and yet the result is, it must be confessed, neither fascinating nor artistic."

"A certain selection and discretion must be used in producing a realistic effect," remarked Holmes. "This is wanting in the police report, where more stress is laid, perhaps, upon the platitudes of the magistrate than upon the details, which to an observer contain the vital essence of the whole matter. Depend upon it, there is nothing so unnatural as the commonplace."

I smiled and shook my head. "I can quite understand your thinking so." I said. "Of course, in your position of unofficial adviser and helper to everybody who is absolutely puzzled, throughout three continents, you are brought in contact with all that is strange and bizarre. But here"--I picked up the morning paper from the ground--"let us put it to a practical test. Here is the first heading upon which I come. 'A husband's cruelty to his wife.' There is half a column of print, but I know without reading it that it is all perfectly familiar to me. There is, of course, the other woman, the drink, the push, the blow, the bruise, the sympathetic sister or landlady. The crudest of writers could invent nothing more crude."

"Indeed, your example is an unfortunate one for your argument," said Holmes, taking the paper and glancing his eye down it. "This is the Dundas separation case, and, as it happens, I was engaged in clearing up some small points in connection with it. The husband was a teetotaler, there was no other woman, and the conduct complained of was that he had drifted into the habit of winding up every meal by taking out his false teeth and hurling them at his wife, which, you will allow, is not an action likely to occur to the imagination of the average story-teller. Take a pinch of snuff, Doctor, and acknowledge that I have scored over you in your example.\"
)");

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

  int yara_callback_function(
    YR_SCAN_CONTEXT* context,
    int message,
    void* message_data,
    void* user_data)
  {
    return CALLBACK_CONTINUE;
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

  REQUIRE(rules->num_rules == 1);
  YR_RULE* rule = &((*rules).rules_table[0]);
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

  int flags = 0;
  BENCHMARK("yaraHolmes") {
    REQUIRE(ERROR_SUCCESS == yr_rules_scan_mem(rules.get(), (uint8_t*)holmes2.c_str(), holmes2.size(), flags, yara_callback_function, nullptr, 0));
  };
}
