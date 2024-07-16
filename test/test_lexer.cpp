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

TEST_CASE("ScanOpenParen") {
  std::string input = "(";
  LlamaLexer lexer(input);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::OPEN_PAREN);
  std::string lexeme(lexer.getLexeme(0));
  REQUIRE(lexeme == "(");
}

TEST_CASE("ScanCloseParen") {
  std::string input = ")";
  LlamaLexer lexer(input);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::CLOSE_PAREN);
  std::string lexeme(lexer.getLexeme(0));
  REQUIRE(lexeme == ")");
}

TEST_CASE("ScanComma") {
  std::string input = ",";
  LlamaLexer lexer(input);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::COMMA);
  std::string lexeme(lexer.getLexeme(0));
  REQUIRE(lexeme == ",");
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

TEST_CASE("parseEncodingsList") {
  std::string input = "=UTF-8,UTF-16";
  LlamaLexer lexer(input);
  lexer.parseEncodingsList();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::EQUAL);
  REQUIRE(lexer.getTokens().at(1).Type == TokenType::ENCODINGS_LIST);
  std::string lexeme(lexer.getLexeme(1));
  REQUIRE(lexeme == "UTF-8,UTF-16");
}

TEST_CASE("parseEncodingsWithoutList") {
  std::string input = "encodings";
  LlamaLexer lexer(input);
  REQUIRE_THROWS_AS(lexer.parseIdentifier(), UnexpectedInputError);
}

TEST_CASE("parseIdentifierEncodingsList") {
  std::string input = "encodings=ASCII,UTF-8";
  LlamaLexer lexer(input);
  lexer.parseIdentifier();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::ENCODINGS);
  REQUIRE(lexer.getTokens().at(1).Type == TokenType::EQUAL);
  REQUIRE(lexer.getTokens().at(2).Type == TokenType::ENCODINGS_LIST);
  std::string lexeme(lexer.getLexeme(2));
  REQUIRE(lexeme == "ASCII,UTF-8");
}

TEST_CASE("parseNocase") {
  std::string input = "nocase";
  LlamaLexer lexer(input);
  lexer.parseIdentifier();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::NOCASE);
}

TEST_CASE("parseFixed") {
  std::string input = "fixed";
  LlamaLexer lexer(input);
  lexer.parseIdentifier();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::FIXED);
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
  std::string input = "file_metadata";
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

TEST_CASE("parseHashId") {
  std::string input = "hash";
  LlamaLexer lexer(input);
  lexer.parseIdentifier();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::HASH);
}

TEST_CASE("parseConditionId") {
  std::string input = "condition";
  LlamaLexer lexer(input);
  lexer.parseIdentifier();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::CONDITION);
}

TEST_CASE("parseCreatedTimeId") {
  std::string input = "created";
  LlamaLexer lexer(input);
  lexer.parseIdentifier();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::CREATED);
}

TEST_CASE("parseModifiedTimeId") {
  std::string input = "modified";
  LlamaLexer lexer(input);
  lexer.parseIdentifier();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::MODIFIED);
}

TEST_CASE("parseFilesizeId") {
  std::string input = "filesize";
  LlamaLexer lexer(input);
  lexer.parseIdentifier();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::FILESIZE);
}

TEST_CASE("parseFilenameId") {
  std::string input = "filename";
  LlamaLexer lexer(input);
  lexer.parseIdentifier();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::FILENAME);
}

TEST_CASE("parseFilepathId") {
  std::string input = "filepath";
  LlamaLexer lexer(input);
  lexer.parseIdentifier();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::FILEPATH);
}

TEST_CASE("parseStringsId") {
  std::string input = "strings";
  LlamaLexer lexer(input);
  lexer.parseIdentifier();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::STRINGS);
}

TEST_CASE("parseAllId") {
  std::string input = "all";
  LlamaLexer lexer(input);
  lexer.parseIdentifier();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::ALL);
}

TEST_CASE("parseAnyId") {
  std::string input = "any";
  LlamaLexer lexer(input);
  lexer.parseIdentifier();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::ANY);
}

TEST_CASE("parseOffsetId") {
  std::string input = "offset";
  LlamaLexer lexer(input);
  lexer.parseIdentifier();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::OFFSET);
}

TEST_CASE("parseCountId") {
  std::string input = "count";
  LlamaLexer lexer(input);
  lexer.parseIdentifier();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::COUNT);
}

TEST_CASE("parseCountHasHitsId") {
  std::string input = "count_has_hits";
  LlamaLexer lexer(input);
  lexer.parseIdentifier();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::COUNT_HAS_HITS);
}

TEST_CASE("parseLengthId") {
  std::string input = "length";
  LlamaLexer lexer(input);
  lexer.parseIdentifier();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::LENGTH);
}

TEST_CASE("parseMd5Id") {
  std::string input = "md5";
  LlamaLexer lexer(input);
  lexer.parseIdentifier();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::MD5);
}

TEST_CASE("parseSha1Id") {
  std::string input = "sha1";
  LlamaLexer lexer(input);
  lexer.parseIdentifier();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::SHA1);
}

TEST_CASE("parseSha256Id") {
  std::string input = "sha256";
  LlamaLexer lexer(input);
  lexer.parseIdentifier();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::SHA256);
}

TEST_CASE("parseBlake3Id") {
  std::string input = "blake3";
  LlamaLexer lexer(input);
  lexer.parseIdentifier();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::BLAKE3);
}

TEST_CASE("parseNotEqual") {
  std::string input = "!=";
  LlamaLexer lexer(input);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::NOT_EQUAL);
  REQUIRE(lexer.isAtEnd());
}

TEST_CASE("parseBangAloneThrowsException") {
  std::string input = "!";
  LlamaLexer lexer(input);
  REQUIRE_THROWS_AS(lexer.scanToken(), UnexpectedInputError);
}

TEST_CASE("parseLessThan") {
  std::string input = "<";
  LlamaLexer lexer(input);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::LESS_THAN);
  REQUIRE(lexer.isAtEnd());
}

TEST_CASE("parseLessThanEqual") {
  std::string input = "<=";
  LlamaLexer lexer(input);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::LESS_THAN_EQUAL);
  REQUIRE(lexer.isAtEnd());
}

TEST_CASE("parseEqual") {
  std::string input = "=";
  LlamaLexer lexer(input);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::EQUAL);
  REQUIRE(lexer.isAtEnd());
}

TEST_CASE("parseEqualEqual") {
  std::string input = "==";
  LlamaLexer lexer(input);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::EQUAL_EQUAL);
  REQUIRE(lexer.isAtEnd());
}

TEST_CASE("parseGreaterThan") {
  std::string input = ">";
  LlamaLexer lexer(input);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::GREATER_THAN);
  REQUIRE(lexer.isAtEnd());
}

TEST_CASE("parseGreaterThanEqual") {
  std::string input = ">=";
  LlamaLexer lexer(input);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(0).Type == TokenType::GREATER_THAN_EQUAL);
  REQUIRE(lexer.isAtEnd());
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

TEST_CASE("match") {
  std::string input = "{";
  LlamaLexer lexer(input);
  REQUIRE(lexer.match('{'));
  REQUIRE_FALSE(lexer.match('{'));
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

TEST_CASE("streamPositionInit") {
  std::string input = "rule";
  LlamaLexer lexer(input);
  REQUIRE(lexer.Pos.LineNum == 1);
  REQUIRE(lexer.Pos.ColNum == 1);
}

TEST_CASE("newLinesIncrementCurPosLineNum") {
  std::string input = "rule\n";
  LlamaLexer lexer(input);
  REQUIRE(lexer.Pos.LineNum == 1);
  REQUIRE(lexer.Pos.ColNum == 1);
  lexer.scanToken();
  REQUIRE(lexer.Pos.ColNum == 5);
  lexer.scanToken();
  REQUIRE(lexer.Pos.LineNum == 2);
  REQUIRE(lexer.Pos.ColNum == 1);
}

TEST_CASE("lone!ThrowsException") {
  std::string input = "!";
  LlamaLexer lexer(input);
  REQUIRE_THROWS_AS(lexer.scanToken(), UnexpectedInputError);
}
