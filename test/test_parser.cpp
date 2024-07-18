#include <catch2/catch_test_macros.hpp>

#include "lexer.h"
#include "parser.h"

std::vector<Token> getTokensFromString(const std::string& input) {
  LlamaLexer lexer(input);
  lexer.scanTokens();
  return lexer.getTokens();
}

Token makeToken(TokenType type) {
  return Token(type, 0, 0, {0, 0});
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
  REQUIRE(parser.checkAny(TokenType::RULE));
}

TEST_CASE("TestLlamaParserCheckFalse") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_FALSE(parser.checkAny(TokenType::OPEN_BRACE));
}

TEST_CASE("TestLlamaParserMatchTrue") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE(parser.matchAny(TokenType::RULE));
  REQUIRE(parser.CurIdx == 1);
}

TEST_CASE("TestLlamaParserMatchFalse") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_FALSE(parser.matchAny(TokenType::OPEN_BRACE));
  REQUIRE(parser.CurIdx == 0);
}

TEST_CASE("TestLlamaParserMatchMultipleTrue") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE(parser.matchAny(TokenType::RULE, TokenType::META));
}

TEST_CASE("TestLlamaParserMatchMultipleFalse") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_FALSE(parser.matchAny(TokenType::OPEN_BRACE, TokenType::META));
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

TEST_CASE("parseHashSectionThrowsIfNotHash") {
  std::string input = "notAHash";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseHashSection(), ParserError);
}

TEST_CASE("parseHashSectionThrowsIfNotColon") {
  std::string input = "hash notColon";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseHashSection(), ParserError);
}

TEST_CASE("parseHashSectionDoesNotThrowIfHashAndColon") {
  std::string input = "hash: md5 = \"test\"";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseHashSection());
}

TEST_CASE("parseArgListThrowsIfNotStartWithIdentifier") {
  std::string input = "hash"; // reserved keyword - not an identifier
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseArgList(), ParserError);
}

TEST_CASE("parseArgListThrowsIfNoIdentifierAfterComma") {
  std::string input = "s1,)"; // no identifier after comma
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseArgList(), ParserError);
}

TEST_CASE("parseArgListDoesNotThrowIfIdentifierAfterComma") {
  std::string input = "s1, s2, s3";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseArgList());
}

TEST_CASE("parseOperatorThrowsIfNotOperator") {
  std::string input = "notAnOperator";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseOperator(), ParserError);
}

TEST_CASE("parseOperatorDoesNotThrowIfOperator") {
  std::string input = "==";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseOperator());
}

TEST_CASE("parseConditionFuncThrowsIfNotConditionFunc") {
  std::string input = "notAConditionFunc";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseConditionFunc(), ParserError);
}

TEST_CASE("parseConditionFuncDoesNotThrowIfConditionFunc") {
  std::string input = "all";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseConditionFunc());
}

TEST_CASE("parseFuncCall") {
  std::string input = "all(s1, s2, s3)";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseFuncCall());
}

TEST_CASE("parseFuncCallThrowsIfNoCloseParen") {
  std::string input = "all(s1, s2, s3";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseFuncCall(), ParserError);
}

TEST_CASE("parseStringModThrowsIfNotStringMod") {
  std::string input = "notAStringMod";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseStringMod(), ParserError);
}

TEST_CASE("parseStringModDoesNotThrowIfStringMod") {
  std::string input = "nocase";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseStringMod());
}

TEST_CASE("parseEncodingsThrowsIfNotEncodings") {
  std::string input = "notEncodings";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseEncodings(), ParserError);
}

TEST_CASE("parseEncodingsDoesNotThrowIfEncodings") {
  std::vector<Token> tokens = {makeToken(TokenType::EQUAL), makeToken(TokenType::ENCODINGS_LIST)};
  LlamaParser parser(tokens);
  REQUIRE_NOTHROW(parser.parseEncodings());
}

TEST_CASE("parseStringDefThrowsIfNotStringDef") {
  std::string input = "rule";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseStringDef(), ParserError);
}

TEST_CASE("parseStringDefDoesNotThrowIfStringDef") {
  std::string input = "a = \"test\" encodings=UTF-8 nocase fixed";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseStringDef());
}

TEST_CASE("parseStringsSectionThrowsIfNotStrings") {
  std::string input = "rule";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseStringsSection(), ParserError);
}

TEST_CASE("parseStringsSectionDoesNotThrowIfStrings") {
  std::string input = "strings:\n  a = \"test\" encodings=UTF-8 nocase fixed\n b = \"test\" encodings=UTF-8 nocase fixed";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseStringsSection());
}

TEST_CASE("parseAnyFuncCall") {
  std::string input = "any(s1, s2, s3)";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseAnyFuncCall());
}

TEST_CASE("parseAllFuncCall") {
  std::string input = "all()";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseAllFuncCall());
}

TEST_CASE("parseDualFuncCall") {
  std::string input = "offset(s1, 5)";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseDualFuncCall());
}

TEST_CASE("parseTermWithAnd") {
  std::string input = "any(s1, s2, s3) and count(s1, 5) == 5";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseTerm());
}

TEST_CASE("parseTermWithoutAnd") {
  std::string input = "any(s1, s2, s3)";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseTerm());
}

TEST_CASE("parseDualFuncCallWithOperator") {
  std::string input = "offset(s1, 5) == 5";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseDualFuncCall());
}

TEST_CASE("parseDualFuncWithoutOperatorThrows") {
  std::string input = "offset(s1, 5)";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseDualFuncCall(), ParserError);
}

TEST_CASE("parseExpr") {
  std::string input = "(any(s1, s2, s3) and count(s1, 5) == 5) or all(s1, s2, s3)";
  LlamaParser parser(getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseExpr());
}