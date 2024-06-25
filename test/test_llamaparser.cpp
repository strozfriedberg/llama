#include <catch2/catch_test_macros.hpp>

#include "lexer.h"

TEST_CASE("ScanToken") {
  std::string input = "{}:= \n\r\t";
  LlamaLexer lexer(input);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::OPEN_BRACE);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(1).Type == TokenType::CLOSE_BRACE);
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
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(1).Type == TokenType::OPEN_BRACE);
}

TEST_CASE("parseString") {
  std::string input = "some string\"";
  LlamaLexer lexer(input);
  lexer.parseString();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::DOUBLE_QUOTED_STRING);
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
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::FILE_METADATA);
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
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::IDENTIFIER);
}

TEST_CASE("parseNumber") {
  std::string input = "123456789";
  LlamaLexer lexer(input);
  lexer.parseNumber();
  std::vector<Token> tokens = lexer.getTokens();
  REQUIRE(tokens.size() == 1);
  REQUIRE(tokens[0].Type == TokenType::NUMBER);
}

TEST_CASE("scanTokens") {
  std::string input = "{ }";
  LlamaLexer lexer(input);
  lexer.scanTokens();
  REQUIRE(lexer.getTokens().size() == 3);
  REQUIRE(lexer.getTokens()[0].Type == TokenType::OPEN_BRACE);
  REQUIRE(lexer.getTokens()[1].Type == TokenType::CLOSE_BRACE);
  REQUIRE(lexer.getTokens()[2].Type == TokenType::END_OF_FILE);
}

TEST_CASE("scanTokensParseIdentifierKeyword") {
  std::string input = "rule";
  LlamaLexer lexer(input);
  lexer.scanTokens();
  REQUIRE(lexer.getTokens().size() == 2);
  REQUIRE(lexer.getTokens()[0].Type == TokenType::RULE);
  REQUIRE(lexer.getTokens()[1].Type == TokenType::END_OF_FILE);
}

TEST_CASE("parseTokensParseIdentifierNonKeyword") {
  std::string input = "foobar";
  LlamaLexer lexer(input);
  lexer.scanTokens();
  REQUIRE(lexer.getTokens().size() == 2);
  REQUIRE(lexer.getTokens()[0].Type == TokenType::IDENTIFIER);
  REQUIRE(lexer.getTokens()[1].Type == TokenType::END_OF_FILE);
}

TEST_CASE("inputIterator") {
  std::string input = "{";
  LlamaLexer lexer(input);
  REQUIRE(lexer.advance() == '{');
  REQUIRE(lexer.isAtEnd());
}

TEST_CASE("scanTokensFullRule") {
  std::string input = "rule MyRule {\n\tmeta:\n\t\tdescription = \"this is my rule\"\nsomething = 5\n}";
  LlamaLexer lexer(input);
  lexer.scanTokens();
  std::vector<Token> tokens = lexer.getTokens();
  REQUIRE(tokens.size() == 13);
  REQUIRE(tokens[0].Type == TokenType::RULE);
  REQUIRE(tokens[1].Type == TokenType::IDENTIFIER);
  REQUIRE(tokens[2].Type == TokenType::OPEN_BRACE);
  REQUIRE(tokens[3].Type == TokenType::META);
  REQUIRE(tokens[4].Type == TokenType::COLON);
  REQUIRE(tokens[5].Type == TokenType::IDENTIFIER);
  REQUIRE(tokens[6].Type == TokenType::EQUAL);
  REQUIRE(tokens[7].Type == TokenType::DOUBLE_QUOTED_STRING);
  REQUIRE(tokens[8].Type == TokenType::IDENTIFIER);
  REQUIRE(tokens[9].Type == TokenType::EQUAL);
  REQUIRE(tokens[10].Type == TokenType::NUMBER);
  REQUIRE(tokens[11].Type == TokenType::CLOSE_BRACE);
  REQUIRE(tokens[12].Type == TokenType::END_OF_FILE);
}
