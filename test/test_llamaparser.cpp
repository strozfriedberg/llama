#include <catch2/catch_test_macros.hpp>

#include "lexer.h"

TEST_CASE("ScanToken") {
  std::string input = "{}:= \n\r\t";
  LlamaLexer lexer(input);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::LCB);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(1).Type == TokenType::RCB);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(2).Type == TokenType::COLON);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(3).Type == TokenType::EQUAL);
  lexer.scanToken();
  lexer.scanToken();
  lexer.scanToken();
  lexer.scanToken();
  REQUIRE(lexer.getTokens().size() == 4);
  REQUIRE(lexer.isAtEnd());
  REQUIRE_THROWS_AS(lexer.scanToken(), UnexpectedInputError);
}

TEST_CASE("ScanTokenString") {
  std::string input = "\"some string\"{";
  LlamaLexer lexer(input);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::DOUBLE_QUOTED_STRING);
  REQUIRE(lexer.getTokens().at(0).Lexeme == "some string");
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(1).Type == TokenType::LCB);
}

TEST_CASE("parseString") {
  std::string input = "some string\"";
  LlamaLexer lexer(input);
  lexer.parseString();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::DOUBLE_QUOTED_STRING);
  REQUIRE(lexer.getTokens().at(0).Lexeme == "some string");
}

TEST_CASE("unterminatedString") {
  std::string input = "some string";
  LlamaLexer lexer(input);
  REQUIRE_THROWS_AS(lexer.parseString(), UnexpectedInputError);
}

TEST_CASE("parseRuleId") {
  std::string input = "rule";
  LlamaLexer lexer(input);
  lexer.parseIdentifier();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::RULE);
}

TEST_CASE("parseMetaId") {
  std::string input = "meta";
  LlamaLexer lexer(input);
  lexer.parseIdentifier();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::META);
}

TEST_CASE("parseFileMetadataId") {
  std::string input = "filemetadata";
  LlamaLexer lexer(input);
  lexer.parseIdentifier();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::FILEMETADATA);
}

TEST_CASE("parseSignatureId") {
  std::string input = "signature";
  LlamaLexer lexer(input);
  lexer.parseIdentifier();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::SIGNATURE);
}

TEST_CASE("parseGrepId") {
  std::string input = "grep";
  LlamaLexer lexer(input);
  lexer.parseIdentifier();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::GREP);
}

TEST_CASE("parseHash") {
  std::string input = "hash";
  LlamaLexer lexer(input);
  lexer.parseIdentifier();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::HASH);
}

TEST_CASE("parseAlphaNumUnderscore") {
  std::string input = "not_a_keyword";
  LlamaLexer lexer(input);
  lexer.parseIdentifier();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::ALPHA_NUM_UNDERSCORE);
}

TEST_CASE("scanTokens") {
  std::string input = "{ }";
  LlamaLexer lexer(input);
  lexer.scanTokens();
  REQUIRE(lexer.getTokens().size() == 3);
  REQUIRE(lexer.getTokens()[0].Type == TokenType::LCB);
  REQUIRE(lexer.getTokens()[1].Type == TokenType::RCB);
  REQUIRE(lexer.getTokens()[2].Type == TokenType::ENDOFFILE);
}

TEST_CASE("inputIterator") {
  std::string input = "{";
  LlamaLexer lexer(input);
  REQUIRE(lexer.advance() == '{');
  REQUIRE(lexer.isAtEnd());
}
