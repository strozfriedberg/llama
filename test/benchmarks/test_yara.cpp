#include <catch2/benchmark/catch_benchmark_all.hpp>
#include <catch2/catch_test_macros.hpp>


#include "yara.h"
#include <lightgrep/api.h>

#include <filesystem>
#include <cstdio>
#include <iostream>

namespace fs = std::filesystem;

namespace {

std::string holmes2(
R"("My dear fellow," said Sherlock Holmes as we sat on either side of the fire in his lodgings at Baker Street, "life is infinitely stranger than anything which the mind of man could invent. We would not dare to conceive the things which are really mere commonplaces of existence. If we could fly out of that window hand in hand, hover over this great city, gently remove the roofs, and peep in at the queer things which are going on, the strange coincidences, the plannings, the cross-purposes, the wonderful chains of events, working through generations, and leading to the most outre results, it would make all fiction with its conventionalities and foreseen conclusions most stale and unprofitable."
"And yet I am not convinced of it," I answered. "The cases which come to light in the papers are, as a rule, bald enough, and vulgar enough. We have in our police reports realism pushed to its extreme limits, and yet the result is, it must be confessed, neither fascinating nor artistic."
"A certain selection and discretion must be used in producing a realistic effect," remarked Holmes. "This is wanting in the police report, where more stress is laid, perhaps, upon the platitudes of the magistrate than upon the details, which to an observer contain the vital essence of the whole matter. Depend upon it, there is nothing so unnatural as the commonplace."
"I smiled and shook my head. "I can quite understand your thinking so." I said. "Of course, in your position of unofficial adviser and helper to everybody who is absolutely puzzled, throughout three continents, you are brought in contact with all that is strange and bizarre. But here"--I picked up the morning paper from the ground--"let us put it to a practical test. Here is the first heading upon which I come. 'A husband's cruelty to his wife.' There is half a column of print, but I know without reading it that it is all perfectly familiar to me. There is, of course, the other woman, the drink, the push, the blow, the bruise, the sympathetic sister or landlady. The crudest of writers could invent nothing more crude."
"Indeed, your example is an unfortunate one for your argument," said Holmes, taking the paper and glancing his eye down it. "This is the Dundas separation case, and, as it happens, I was engaged in clearing up some small points in connection with it. The husband was a teetotaler, there was no other woman, and the conduct complained of was that he had drifted into the habit of winding up every meal by taking out his false teeth and hurling them at his wife, which, you will allow, is not an action likely to occur to the imagination of the average story-teller. Take a pinch of snuff, Doctor, and acknowledge that I have scored over you in your example.\"
)");

std::string ASCII_RULE = R"(
rule APT15_Malware_Mar18_RoyalCliPartial {
   meta:
      description = "Detects malware from APT 15 report by NCC Group"
      license = "Detection Rule License 1.1 https://github.com/Neo23x0/signature-base/blob/master/LICENSE"
      author = "Florian Roth - Nextron Systems"
      reference = "https://goo.gl/HZ5XMN"
      date = "2018-03-10"
      hash1 = "6df9b712ff56009810c4000a0ad47e41b7a6183b69416251e060b5c80cd05785"
      id = "165bfa6c-1a8d-5628-8c35-da4e4a2ae04f"
   strings:
      $s1 = "Run cmd error %d" ascii wide fullword
      $s2 = "run file failed" ascii wide
      $s3 = "\\Release\\RoyalCli.pdb" ascii
   condition:
      uint16(0) == 0x5a4d and filesize < 200KB and 2 of them
})";

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

struct YaraPattern {
  std::string Expression;
};

std::string byteToHexString(uint8_t b) {
  std::string ret("\\z");
  ret += hex_char(b >> 4);
  ret += hex_char(b & 0x0f);
  return ret;
}

std::string ESCAPED_CHARS = "(){}[]*-+?\\";

std::string convertChar(uint8_t b) {
  std::string ret;
  if (std::isprint(b)) {
    if (ESCAPED_CHARS.find(b) != std::string::npos) {
      ret = "\\";
    }
    ret += char(b);
  }
  else {
    ret = byteToHexString(b);
  }
  return ret;
}

std::vector<YaraPattern> yaraToLG(int flags, uint8_t* bytes, unsigned int length) {
  std::vector<YaraPattern> yps;
  if (flags & STRING_FLAGS_HEXADECIMAL) {
    YaraPattern yp;
    for (unsigned int i = 0; i < length; ++i) {
      yp.Expression += byteToHexString(bytes[i]);
    }
    yps.push_back(yp);
    return yps;
  }
  if (flags & STRING_FLAGS_ASCII) {
    YaraPattern yp;
    if (flags & STRING_FLAGS_FULL_WORD) {
      yp.Expression = "\\W";
    }
    for (unsigned int i = 0; i < length; ++i) {
      yp.Expression += convertChar(bytes[i]);
    }
    if (flags & STRING_FLAGS_FULL_WORD) {
      yp.Expression += "\\W";
    }
    yps.push_back(yp);
  }
  if (flags & STRING_FLAGS_WIDE) {
    YaraPattern yp;
    if (flags & STRING_FLAGS_FULL_WORD) {
      yp.Expression = "\\W\\z00";
    }
    for (unsigned int i = 0; i < length; ++i) {
      yp.Expression += convertChar(bytes[i]);
      yp.Expression += "\\z00";
    }
    if (flags & STRING_FLAGS_FULL_WORD) {
      yp.Expression += "\\W\\z00";
    }
    yps.push_back(yp);
  }
  return yps;
}

class YaraLib {
// don't make more than one of these at a time... it would be bad
public:
  YaraLib() {
    yr_initialize();
    YR_COMPILER* c = nullptr;
    yr_compiler_create(&c);
    Comp.reset(c, yr_compiler_destroy);
  }

  ~YaraLib() {
    Comp.reset();
    yr_finalize();
  }

  bool compile(void) {
    YR_RULES* rulesPtr = nullptr;
    int ret = yr_compiler_get_rules(Comp.get(), &rulesPtr);
    Rules.reset(rulesPtr, yr_rules_destroy);
    return ret == 0;
  }

  std::shared_ptr<YR_COMPILER> Comp;
  std::shared_ptr<YR_RULES> Rules;

  bool addRulesFromString(const std::string& s) {
    return 0 == yr_compiler_add_string(Comp.get(), s.c_str(), "");
  }

  unsigned int addYaraRulesFromPath(const std::string& path) {
    unsigned int fileCount = 0;

    // WARN("addYaraRulesFromPath(" << path << ")");
    //unsigned int i = 0;
    for (const auto& dir_entry: fs::recursive_directory_iterator(path)) {
      //++i;
      // auto yarpath = dir_entry.path();
      std::string ext = dir_entry.path().extension().string();
      std::string yarpath = dir_entry.path().string();
      // WARN("path = " << yarpath);
      if (ext.compare(".yar") != 0) {
        continue;
      }
      // WARN("Opening " << yarpath);
      FILE* f = std::fopen(yarpath.c_str(), "rb");
      if (!f) {
        WARN("Couldn't open " << dir_entry << '\n');
        continue;
      }
      INFO("Adding " << yarpath << " to yara compiler");
      yr_compiler_add_file(Comp.get(), f, "", yarpath.c_str());
      ++fileCount;
    }
    // WARN("i = " << i);
    return fileCount;
  }

  std::vector<YaraPattern> convertToLG() {
    std::vector<YaraPattern> ret;
    YR_RULE* rule = nullptr;
    // WARN("yr_rules_foreach");
    yr_rules_foreach(Rules.get(), rule) {
      YR_STRING* str = nullptr;
      yr_rule_strings_foreach(rule, str) {
        CAPTURE(str->length);
        CAPTURE(STRING_IS_HEX(str));
        auto lgPats = yaraToLG(str->flags, str->string, str->length);
        ret.insert(ret.end(), lgPats.begin(), lgPats.end());
      }
    }
    return ret;
  }
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

unsigned int g_yaraCallbackCount = 0;
unsigned int g_lgCallbackCount = 0;

int yara_callback_function(
  YR_SCAN_CONTEXT* /*context*/,
  int /*message*/,
  void* /*message_data*/,
  void* /*user_data*/)
{
  ++g_yaraCallbackCount;
  return CALLBACK_CONTINUE;
}

void lg_callback_function(
  void* /*userData*/,
  const LG_SearchHit* const /*hit*/)
{
  ++g_lgCallbackCount;
}

} // local namespace

TEST_CASE("yaraHexToLG") {
  uint8_t input[] = {0x73, 0x70, 0x72, 0x6E, 0x67, 0x00};

  auto yp = yaraToLG(STRING_FLAGS_HEXADECIMAL, input, 6);
  REQUIRE(yp.size() == 1);
  REQUIRE(R"(\z73\z70\z72\z6e\z67\z00)" == yp[0].Expression);
}

TEST_CASE("yaraAsciiToLG") {
  YaraLib yara;

  REQUIRE(yara.addRulesFromString(ASCII_RULE));
  REQUIRE(yara.compile());
  auto patterns = yara.convertToLG();
  REQUIRE(patterns.size() == 5);
  CHECK(patterns[0].Expression == "\\WRun cmd error %d\\W");
  CHECK(patterns[1].Expression == "\\W\\z00R\\z00u\\z00n\\z00 \\z00c\\z00m\\z00d\\z00 \\z00e\\z00r\\z00r\\z00o\\z00r\\z00 \\z00%\\z00d\\z00\\W\\z00");
  CHECK(patterns[2].Expression == "run file failed");
  CHECK(patterns[3].Expression == "r\\z00u\\z00n\\z00 \\z00f\\z00i\\z00l\\z00e\\z00 \\z00f\\z00a\\z00i\\z00l\\z00e\\z00d\\z00");
  CHECK(patterns[4].Expression == "\\\\Release\\\\RoyalCli.pdb");
}

  // yr_rules_scan_mem(yara.Rules.get(), (uint8_t*)holmes2.c_str(), holmes2.size(), flags, yara_callback_function, nullptr, 0);
  // REQUIRE(g_yaraCallbackCount == 13);

  // lg_search(ctx, holmes2.data(), holmes2.data() + holmes2.size(), 0, nullptr, lg_callback_function);
  // REQUIRE(g_lgCallbackCount == 0);


//TEST_CASE("asanTest") {
//  const char *s = "hello, there, what an odd world we live in.";
//
//  BENCHMARK("memchr") {
//    return std::memchr(s, 'v', 43);
//  };
//}

TEST_CASE("testYara") {
  YaraLib yara;

  REQUIRE(0 < yara.addYaraRulesFromPath("./test/data/yara"));

  REQUIRE(yara.compile());

  CHECK(yara.Rules->num_rules == 11);

  std::vector<YaraPattern> patterns = yara.convertToLG();
  CHECK(patterns.size() == 344);

  int flags = 0;
  BENCHMARK("yaraHolmes") {
    yr_rules_scan_mem(yara.Rules.get(), (uint8_t*)holmes2.c_str(), holmes2.size(), flags, yara_callback_function, nullptr, 0);
  };

  LG_HPATTERN pat = lg_create_pattern();
  LG_HFSM fsm = lg_create_fsm(2, 30);
  LG_Error* errPtr = nullptr;
  LG_KeyOptions keyOpts{0, 0, 0};
  unsigned int i = 0;
  for (const auto& p: patterns) {
    INFO("lg_parse_pattern(\"" << p.Expression << "\")");
    if (p.Expression.empty()) {
      continue;
    }
    if (0 == lg_parse_pattern(pat, p.Expression.c_str(), &keyOpts, &errPtr)) {
      if (p.Expression.empty()) {
        WARN("could not parse null expression");
      }
      if (errPtr) {
        WARN("could not parse '" << p.Expression << "': " << (errPtr->Message ? errPtr->Message: "unspecified"));
      }
      continue;
    }
    if (lg_add_pattern(fsm, pat, "ISO-8859-1", i++, &errPtr) < 0) {
      WARN("could not add pattern '" << p.Expression << "' to FSM: " << errPtr->Message);
    }
  }
  LG_ProgramOptions progOpts{10};
  LG_HPROGRAM prog = lg_create_program(fsm, &progOpts);
  REQUIRE(prog);

  LG_ContextOptions ctxOpts{0, 0};
  LG_HCONTEXT ctx = lg_create_context(prog, &ctxOpts);
  REQUIRE(ctx);

  BENCHMARK("lightgrepHolmes") {
    lg_search(ctx, holmes2.data(), holmes2.data() + holmes2.size(), 0, nullptr, lg_callback_function);
  };
}
