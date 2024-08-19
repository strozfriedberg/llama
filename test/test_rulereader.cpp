#include <catch2/catch_test_macros.hpp>

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
  REQUIRE(reader.getLastError() == "Expected rule name at line 1 column 6");
  result = reader.read(input3);
  REQUIRE(result == 3);
}