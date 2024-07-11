#include <catch2/catch_test_macros.hpp>

#include "lexer.h"

class LlamaParser {
public:
  LlamaParser(std::vector<Token> tokens) : Tokens(tokens) {}

  Token previous() { return Tokens.at(CurIdx - 1); }
  Token peek() const { return Tokens.at(CurIdx); }

  std::vector<Token> Tokens;
  uint32_t CurIdx = 0;
};

TEST_CASE("LlamaParser") {
  std::vector<Token> tokens;
  LlamaParser parser(tokens);
}

TEST_CASE("TestLlamaParserPrevious") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaLexer lexer(input);
  lexer.scanTokens();
  LlamaParser parser(lexer.getTokens());
  parser.CurIdx = 1;
  REQUIRE(parser.previous().Type == TokenType::RULE);
}

TEST_CASE("TestLlamaParserPeek") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaLexer lexer(input);
  lexer.scanTokens();
  LlamaParser parser(lexer.getTokens());
  parser.CurIdx = 1;
  REQUIRE(parser.peek().Type == TokenType::OPEN_BRACE);
}