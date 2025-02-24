#include <catch2/catch_test_macros.hpp>

#include <sstream>

#include "llama.h"
#include "ruleengine.h"

TEST_CASE("testReadDirPopulatesRulesCorrectly") {
  std::string testDir = "test/rules";
  LlamaRuleEngine engine;
  readRulesFromDir(engine, testDir);
  REQUIRE(engine.numRulesRead() == 2);
}
