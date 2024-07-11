#include <catch2/catch_test_macros.hpp>

#include "lexer.h"
#include "parser.h"

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

TEST_CASE("TestLlamaParserIsAtEndTrue") {
  std::string input = "rule";
  LlamaParser parser(getTokensFromString(input));
  parser.advance();
  REQUIRE(parser.isAtEnd());
}

TEST_CASE("TestLlamaParserIsAtEndFalse") {
  std::string input = "rule";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_FALSE(parser.isAtEnd());
}

TEST_CASE("TestLlamaParserAdvance") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(getTokensFromString(input));
  parser.advance();
  REQUIRE(parser.previous().Type == TokenType::RULE);
  REQUIRE(parser.peek().Type == TokenType::OPEN_BRACE);
}

TEST_CASE("TestLlamaParserCheckTrue") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE(parser.check(TokenType::RULE));
}

TEST_CASE("TestLlamaParserCheckFalse") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_FALSE(parser.check(TokenType::OPEN_BRACE));
}

TEST_CASE("TestLlamaParserMatchTrue") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE(parser.match(TokenType::RULE));
  REQUIRE(parser.CurIdx == 1);
}

TEST_CASE("TestLlamaParserMatchFalse") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_FALSE(parser.match(TokenType::OPEN_BRACE));
  REQUIRE(parser.CurIdx == 0);
}

TEST_CASE("TestLlamaParserMatchMultipleTrue") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE(parser.match(TokenType::RULE, TokenType::META));
}

TEST_CASE("TestLlamaParserMatchMultipleFalse") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_FALSE(parser.match(TokenType::OPEN_BRACE, TokenType::META));
}

TEST_CASE("parseHashThrowsIfNotHash") {
  std::string input = "notAHash";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseHash(), ParserError);
}

TEST_CASE("parseHashDoesNotThrowIfHash") {
  std::string input = "md5";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseHash());
}

TEST_CASE("parseHashExprThrowsIfNotEqual") {
  std::string input = "md5 notEqual";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseHashExpr(), ParserError);
}

TEST_CASE("parseHashExprThrowsIfNotDoubleQuotedString") {
  std::string input = "md5 = notDoubleQuotedString";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseHashExpr(), ParserError);
}

TEST_CASE("parseHashExprDoesNotThrowIfEqualAndDoubleQuotedString") {
  std::string input = "md5 = \"test\"";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseHashExpr());
}