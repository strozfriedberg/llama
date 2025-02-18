#include <catch2/catch_test_macros.hpp>

#include "llama.h"
#include "ruleengine.h"

TEST_CASE("testReadDirPopulatesRulesCorrectly") {
  std::string test_dir = "test/rules";
  LlamaRuleEngine engine;
  readRulesFromDir(engine, test_dir);
  REQUIRE(engine.numRulesRead() == 2);
}