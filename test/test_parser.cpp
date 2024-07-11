#include <catch2/catch_test_macros.hpp>

#include "lexer.h"

class LlamaParser {
public:
  LlamaParser(std::vector<Token> tokens) : Tokens(tokens) {}

  Token previous() const { return Tokens.at(CurIdx - 1); }
  Token peek() const { return Tokens.at(CurIdx); }
  Token advance() { if (!isAtEnd()) ++CurIdx; return previous();}

  bool check(TokenType type) const { return peek().Type == type; }
  bool isAtEnd() const { return peek().Type == TokenType::END_OF_FILE; }

  std::vector<Token> Tokens;
  uint32_t CurIdx = 0;
};


std::vector<Token> getTokensFromString(const std::string& input) {
  LlamaLexer lexer(input);
  lexer.scanTokens();
  return lexer.getTokens();
}

TEST_CASE("LlamaParser") {
  std::vector<Token> tokens;
  LlamaParser parser(tokens);
}

TEST_CASE("TestLlamaParserPrevious") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(getTokensFromString(input));
  parser.CurIdx = 1;
  REQUIRE(parser.previous().Type == TokenType::RULE);
}

TEST_CASE("TestLlamaParserPeek") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(getTokensFromString(input));
  parser.CurIdx = 1;
  REQUIRE(parser.peek().Type == TokenType::OPEN_BRACE);
}

TEST_CASE("TestLlamaParserIsAtEnd") {
  std::string input = "rule";
  LlamaParser parser(getTokensFromString(input));
  parser.CurIdx = 1;
  REQUIRE(parser.isAtEnd());
}

TEST_CASE("TestLlamaParserAdvance") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(getTokensFromString(input));
  parser.advance();
  REQUIRE(parser.previous().Type == TokenType::RULE);
  REQUIRE(parser.peek().Type == TokenType::OPEN_BRACE);
}

TEST_CASE("TestLlamaParserCheck") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(getTokensFromString(input));
  parser.CurIdx = 1;
  REQUIRE(parser.check(TokenType::OPEN_BRACE));
}