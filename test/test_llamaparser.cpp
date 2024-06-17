#include <catch2/catch_test_macros.hpp>

#include <vector>

#include <llama_parser.h>

TEST_CASE("ParseBasicRule") {
  char* input = "rule { meta: some_id = \"some_value\" }";
  LlamaLexer lexer(input);
  REQUIRE(lexer.getTokens().size() == 8);
  REQUIRE(lexer.getTokens()[0]->getType() == TokenType::RULE);
  REQUIRE(lexer.getTokens()[1]->getType() == TokenType::LCB);
  REQUIRE(lexer.getTokens()[2]->getType() == TokenType::META);
  REQUIRE(lexer.getTokens()[3]->getType() == TokenType::COLON);
  REQUIRE(lexer.getTokens()[4]->getType() == TokenType::ALPHA_NUM_UNDERSCORE);
  REQUIRE(lexer.getTokens()[5]->getType() == TokenType::EQUAL);
  REQUIRE(lexer.getTokens()[6]->getType() == TokenType::DOUBLE_QUOTED_STRING);
  REQUIRE(lexer.getTokens()[7]->getType() == TokenType::RCB);
}