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
  std::string input = "";
  LlamaParser parser(input, tokens);
}

TEST_CASE("TestLlamaParserPrevious") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(input, getTokensFromString(input));
  parser.CurIdx = 1;
  REQUIRE(parser.previous().Type == TokenType::RULE);
}

TEST_CASE("TestLlamaParserPeek") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(input, getTokensFromString(input));
  parser.CurIdx = 1;
  REQUIRE(parser.peek().Type == TokenType::OPEN_BRACE);
}

TEST_CASE("TestLlamaParserIsAtEndTrue") {
  std::string input = "rule";
  LlamaParser parser(input, getTokensFromString(input));
  parser.advance();
  REQUIRE(parser.isAtEnd());
}

TEST_CASE("TestLlamaParserIsAtEndFalse") {
  std::string input = "rule";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_FALSE(parser.isAtEnd());
}

TEST_CASE("TestLlamaParserAdvance") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(input, getTokensFromString(input));
  parser.advance();
  REQUIRE(parser.previous().Type == TokenType::RULE);
  REQUIRE(parser.peek().Type == TokenType::OPEN_BRACE);
}

TEST_CASE("TestLlamaParserCheckTrue") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE(parser.checkAny(TokenType::RULE));
}

TEST_CASE("TestLlamaParserCheckFalse") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_FALSE(parser.checkAny(TokenType::OPEN_BRACE));
}

TEST_CASE("TestLlamaParserMatchTrue") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE(parser.matchAny(TokenType::RULE));
  REQUIRE(parser.CurIdx == 1);
}

TEST_CASE("TestLlamaParserMatchFalse") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_FALSE(parser.matchAny(TokenType::OPEN_BRACE));
  REQUIRE(parser.CurIdx == 0);
}

TEST_CASE("TestLlamaParserMatchMultipleTrue") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE(parser.matchAny(TokenType::RULE, TokenType::META));
}

TEST_CASE("TestLlamaParserMatchMultipleFalse") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_FALSE(parser.matchAny(TokenType::OPEN_BRACE, TokenType::META));
}

TEST_CASE("parseHashThrowsIfNotHash") {
  std::string input = "notAHash";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseHash(), ParserError);
}

TEST_CASE("parseHashDoesNotThrowIfHash") {
  std::string input = "md5";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseHash());
}

TEST_CASE("parseHashExprThrowsIfNotEqual") {
  std::string input = "md5 notEqual";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseHashExpr(), ParserError);
}

TEST_CASE("parseHashExprThrowsIfNotDoubleQuotedString") {
  std::string input = "md5 = notDoubleQuotedString";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseHashExpr(), ParserError);
}

TEST_CASE("parseHashExprDoesNotThrowIfEqualAndDoubleQuotedString") {
  std::string input = "md5 = \"test\"";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseHashExpr());
}

TEST_CASE("parseHashSectionThrowsIfNotHash") {
  std::string input = "notAHash";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseHashSection(), ParserError);
}

TEST_CASE("parseHashSectionThrowsIfNotColon") {
  std::string input = "hash notColon";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseHashSection(), ParserError);
}

TEST_CASE("parseHashSectionDoesNotThrowIfHashAndColon") {
  std::string input = "hash: md5 = \"test\"";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseHashSection());
}

TEST_CASE("parseOperatorThrowsIfNotOperator") {
  std::string input = "notAnOperator";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseOperator(), ParserError);
}

TEST_CASE("parseOperatorDoesNotThrowIfOperator") {
  std::string input = "==";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseOperator());
}

TEST_CASE("parseStringModThrowsIfNotStringMod") {
  std::string input = "notAStringMod";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parsePatternMod(), ParserError);
}

TEST_CASE("parseStringModDoesNotThrowIfStringMod") {
  std::string input = "nocase";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parsePatternMod());
}

TEST_CASE("parseEncodingsThrowsIfNotEncodings") {
  std::string input = "notEncodings";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseEncodings(), ParserError);
}

TEST_CASE("parseEncodingsDoesNotThrowIfEncodings") {
  std::string input = "encodings=UTF-8";
  std::vector<Token> tokens = {makeToken(TokenType::EQUAL), makeToken(TokenType::ENCODINGS_LIST)};
  LlamaParser parser(input, tokens);
  REQUIRE_NOTHROW(parser.parseEncodings());
}

TEST_CASE("parseStringDefThrowsIfNotStringDef") {
  std::string input = "rule";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parsePatternDef(), ParserError);
}

TEST_CASE("parseStringDefDoesNotThrowIfStringDef") {
  std::string input = "a = \"test\" encodings=UTF-8 nocase fixed";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parsePatternDef());
}

TEST_CASE("parsePatternsSectionThrowsIfNotPatterns") {
  std::string input = "rule";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parsePatternsSection(), ParserError);
}

TEST_CASE("parsePatternsSectionDoesNotThrowIfPatterns") {
  std::string input = "patterns:\n  a = \"test\" encodings=UTF-8 nocase fixed\n b = \"test\" encodings=UTF-8 nocase fixed";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parsePatternsSection());
}

TEST_CASE("parsePatternsSectionWithHexStrings") {
  std::string input = "patterns:\n  a = 0x1234\nb = \"test\" encodings=UTF-8 nocase fixed";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parsePatternsSection());
}

TEST_CASE("parseAnyFuncCall") {
  std::string input = "any(s1, s2, s3)";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseAnyFuncCall());
}

TEST_CASE("parseAllFuncCall") {
  std::string input = "all()";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseAllFuncCall());
}

TEST_CASE("parseTermWithAnd") {
  std::string input = "any(s1, s2, s3) and count(s1, 5) == 5";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseTerm());
}

TEST_CASE("parseTermWithoutAnd") {
  std::string input = "any(s1, s2, s3)";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseTerm());
}

TEST_CASE("parseDualFuncCallWithOperator") {
  std::string input = "offset(s1, 5) == 5";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseDualFuncCall());
}

TEST_CASE("parseDualFuncWithoutOperatorThrows") {
  std::string input = "offset(s1, 5)";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseDualFuncCall(), ParserError);
}

TEST_CASE("parseExpr") {
  std::string input = "(any(s1, s2, s3) and count(s1, 5) == 5) or all(s1, s2, s3)";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseExpr());
}

TEST_CASE("parseConditionSection") {
  std::string input = "condition:\n  (any(s1, s2, s3) and count(s1, 5) == 5)\nor all(s1, s2, s3)";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseConditionSection());
}

TEST_CASE("parseSignatureSection") {
  std::string input = "signature:\n \"EXE\"\n\"MUI\"";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseSignatureSection());
}

TEST_CASE("parseGrepSection") {
  std::string input = R"(
  grep:
    patterns:
      a = "test" encodings=UTF-8 nocase fixed
      b = "test2" encodings=UTF-8 nocase fixed
    condition:
      any(a, b) and offset(a, 5) == 5
)";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseGrepSection());
}

TEST_CASE("parseFileMetadataDefFileSize") {
  std::string input = "filesize > 100";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseFileMetadataDef());
}

TEST_CASE("parseFileMetadataDefCreated") {
  std::string input = "created >= \"2024-05-06\"";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseFileMetadataDef());
}

TEST_CASE("parseFileMetadataDefModified") {
  std::string input = "modified >= \"2024-05-06\"";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseFileMetadataDef());
}

TEST_CASE("parseFileMetadataSection") {
  std::string input = R"(
  file_metadata:
    created > "2023-05-04"
    modified < "2023-05-06"
    filesize >= 100
  )";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseFileMetadataSection());
}

TEST_CASE("parseMetaSection") {
  std::string input = R"(
  meta:
    arbitrary = "something"
    another = "something else"
  )";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseMetaSection());
}

TEST_CASE("parseNonGrepSectionHash") {
  std::string input = R"(
  hash: md5 = "test"
  )";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseNonGrepSection());
}

TEST_CASE("parseNonGrepSectionSignature") {
  std::string input = R"(
  signature:
    "EXE"
    "MUI"
  )";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseNonGrepSection());
}

TEST_CASE("parseNonGrepSectionFileMetadata") {
  std::string input = R"(
  file_metadata:
    created > "2023-05-04"
    modified < "2023-05-06"
    filesize >= 100
  )";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseNonGrepSection());
}

TEST_CASE("parseNonGrepSectionThrows") {
  std::string input = "notHashOrSignatureOrFileMetadata";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseNonGrepSection(), ParserError);
}

TEST_CASE("parseRuleContentGrep") {
  std::string input = R"(
  grep:
    patterns:
      a = "test" encodings=UTF-8 nocase fixed
      b = "test2" encodings=UTF-8 nocase fixed
    condition:
      any(a, b) and offset(a, 5) == 5
  )";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseRuleContent());
}

TEST_CASE("parseRuleContentNonGrep") {
  std::string input = R"(
  hash: md5 = "test"
  signature: "EXE"
  file_metadata: created > "2023-05-04"
  )";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseRuleContent());
}

TEST_CASE("parseRule") {
  std::string input = R"(
  rule:
    meta:
      description = "test"
    signature:
      "EXE"
    file_metadata:
      created > "2023-05-04"
      modified < "2023-05-06"
      filesize >= 100
  )";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseRule());
}

TEST_CASE("parseRuleWithoutMeta") {
  std::string input = R"(
  signature:
      "EXE"
  file_metadata:
    created > "2023-05-04"
    modified < "2023-05-06"
    filesize >= 100
  )";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseRule());
}

TEST_CASE("parseRuleDecl") {
  std::string input = R"(
  rule MyRule {
    meta:
      description = "test"
    signature:
      "EXE"
    file_metadata:
      created > "2023-05-04"
      modified < "2023-05-06"
      filesize >= 100
  }
  )";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseRuleDecl());
}

TEST_CASE("parseRuleDeclThrowsIfBothGrepAndNonGrepSection") {
  std::string input = R"(
  rule MyRule {
    grep:
      patterns:
        a = "test" encodings=UTF-8 nocase fixed
        b = "test2" encodings=UTF-8 nocase fixed
      condition:
        any(a, b) and offset(a, 5) == 5
    hash: md5 = "test"
  }
  )";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseRuleDecl(), ParserError);
}

TEST_CASE("startRule") {
  std::string input = R"(
  rule MyRule {
    meta:
      description = "test"
    signature:
      "EXE"
  }
  rule AnotherRule {
    meta:
      description = "test"
    grep:
      patterns:
        a = "test" encodings=UTF-8 nocase fixed
        b = "test2" encodings=UTF-8 nocase fixed
      condition:
        any(a, b) and count(a) == 5
  }
  )";
  LlamaParser parser(input, getTokensFromString(input));
  std::vector<Rule> rules;
  REQUIRE_NOTHROW(rules = parser.parseRules());
  REQUIRE(rules.size() == 2);
  REQUIRE(rules.at(0).Name == "MyRule");
  REQUIRE(rules.at(1).Name == "AnotherRule");
}
