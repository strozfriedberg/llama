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

// TEST_CASE("testReadDirFailsIfPathIsNotDir") {
//   std::string testDir = "test/rules/test_rule.llama";
//   LlamaRuleEngine engine;

//   std::stringstream cerrBuffer;
//   std::streambuf* originalCerr = std::cerr.rdbuf();
//   std::cerr.rdbuf(cerrBuffer.rdbuf());

//   REQUIRE_FALSE(readRulesFromDir(engine, testDir));
//   std::cerr.rdbuf(originalCerr);
//   REQUIRE(cerrBuffer.str() == "Error: " + testDir + " is not a directory\n");
// }