#include <catch2/catch_test_macros.hpp>

#include "lexer.h"

TEST_CASE("ScanToken") {
  std::string input = "{}:= \n\r\t";
  LlamaLexer lexer(input);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(0).Type == LlamaTokenType::OPEN_BRACE);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(1).Type == LlamaTokenType::CLOSE_BRACE);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(2).Type == LlamaTokenType::COLON);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(3).Type == LlamaTokenType::EQUAL);
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
  REQUIRE(lexer.getTokens().at(0).Type == LlamaTokenType::OPEN_PAREN);
}

TEST_CASE("ScanCloseParen") {
  std::string input = ")";
  LlamaLexer lexer(input);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(0).Type == LlamaTokenType::CLOSE_PAREN);
}

TEST_CASE("ScanComma") {
  std::string input = ",";
  LlamaLexer lexer(input);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(0).Type == LlamaTokenType::COMMA);
}

TEST_CASE("ScanTokenString") {
  std::string input = "\"some string\"{";
  LlamaLexer lexer(input);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(0).Type == LlamaTokenType::DOUBLE_QUOTED_STRING);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(1).Type == LlamaTokenType::OPEN_BRACE);
}

TEST_CASE("parseString") {
  std::string input = "\"some string\"";
  LlamaLexer lexer(input);
  lexer.scanTokens();
  REQUIRE(lexer.getTokens().at(0).Type == LlamaTokenType::DOUBLE_QUOTED_STRING);
  REQUIRE(lexer.getTokens().at(0).Lexeme == "some string");
}

TEST_CASE("parseEncodingsList") {
  std::string input = "encodings=UTF-8,UTF-16";
  LlamaLexer lexer(input);
  lexer.scanTokens();
  REQUIRE(lexer.getTokens().size() == 6);
  REQUIRE(lexer.getTokens().at(0).Type == LlamaTokenType::ENCODINGS);
  REQUIRE(lexer.getTokens().at(1).Type == LlamaTokenType::EQUAL);
  REQUIRE(lexer.getTokens().at(2).Type == LlamaTokenType::IDENTIFIER);
  REQUIRE(lexer.getTokens().at(3).Type == LlamaTokenType::COMMA);
  REQUIRE(lexer.getTokens().at(4).Type == LlamaTokenType::IDENTIFIER);
  REQUIRE(lexer.getTokens().at(5).Type == LlamaTokenType::END_OF_FILE);
}

TEST_CASE("parseNocase") {
  std::string input = "nocase";
  LlamaLexer lexer(input);
  lexer.parseIdentifier({0,0});
  REQUIRE(lexer.getTokens().at(0).Type == LlamaTokenType::NOCASE);
}

TEST_CASE("parseFixed") {
  std::string input = "fixed";
  LlamaLexer lexer(input);
  lexer.parseIdentifier({0,0});
  REQUIRE(lexer.getTokens().at(0).Type == LlamaTokenType::FIXED);
}

TEST_CASE("unterminatedString") {
  std::string input = "some string";
  LlamaLexer lexer(input);
  REQUIRE_THROWS_AS(lexer.parseString({0,0}), UnexpectedInputError);
}

TEST_CASE("parseRuleId") {
  std::string input = "rule";
  LlamaLexer lexer(input);
  lexer.parseIdentifier({0,0});
  REQUIRE(lexer.getTokens().at(0).Type == LlamaTokenType::RULE);
}

TEST_CASE("parseMetaId") {
  std::string input = "meta";
  LlamaLexer lexer(input);
  lexer.parseIdentifier({0,0});
  REQUIRE(lexer.getTokens().at(0).Type == LlamaTokenType::META);
}

TEST_CASE("parseFileMetadataId") {
  std::string input = "file_metadata";
  LlamaLexer lexer(input);
  lexer.parseIdentifier({0,0});
  REQUIRE(lexer.getTokens().at(0).Type == LlamaTokenType::FILE_METADATA);
}

TEST_CASE("parseSignatureId") {
  std::string input = "signature";
  LlamaLexer lexer(input);
  lexer.parseIdentifier({0,0});
  REQUIRE(lexer.getTokens().at(0).Type == LlamaTokenType::SIGNATURE);
}

TEST_CASE("parseGrepId") {
  std::string input = "grep";
  LlamaLexer lexer(input);
  lexer.parseIdentifier({0,0});
  REQUIRE(lexer.getTokens().at(0).Type == LlamaTokenType::GREP);
}

TEST_CASE("parseHashId") {
  std::string input = "hash";
  LlamaLexer lexer(input);
  lexer.parseIdentifier({0,0});
  REQUIRE(lexer.getTokens().at(0).Type == LlamaTokenType::HASH);
}

TEST_CASE("parseConditionId") {
  std::string input = "condition";
  LlamaLexer lexer(input);
  lexer.parseIdentifier({0,0});
  REQUIRE(lexer.getTokens().at(0).Type == LlamaTokenType::CONDITION);
}

TEST_CASE("parseMd5Id") {
  std::string input = "md5";
  LlamaLexer lexer(input);
  lexer.parseIdentifier({0,0});
  REQUIRE(lexer.getTokens().at(0).Type == LlamaTokenType::MD5);
}

TEST_CASE("parseSha1Id") {
  std::string input = "sha1";
  LlamaLexer lexer(input);
  lexer.parseIdentifier({0,0});
  REQUIRE(lexer.getTokens().at(0).Type == LlamaTokenType::SHA1);
}

TEST_CASE("parseSha256Id") {
  std::string input = "sha256";
  LlamaLexer lexer(input);
  lexer.parseIdentifier({0,0});
  REQUIRE(lexer.getTokens().at(0).Type == LlamaTokenType::SHA256);
}

TEST_CASE("parseBlake3Id") {
  std::string input = "blake3";
  LlamaLexer lexer(input);
  lexer.parseIdentifier({0,0});
  REQUIRE(lexer.getTokens().at(0).Type == LlamaTokenType::BLAKE3);
}

TEST_CASE("parseNotEqual") {
  std::string input = "!=";
  LlamaLexer lexer(input);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(0).Type == LlamaTokenType::NOT_EQUAL);
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
  REQUIRE(lexer.getTokens().at(0).Type == LlamaTokenType::LESS_THAN);
  REQUIRE(lexer.isAtEnd());
}

TEST_CASE("parseLessThanEqual") {
  std::string input = "<=";
  LlamaLexer lexer(input);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(0).Type == LlamaTokenType::LESS_THAN_EQUAL);
  REQUIRE(lexer.isAtEnd());
}

TEST_CASE("parseEqual") {
  std::string input = "=";
  LlamaLexer lexer(input);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(0).Type == LlamaTokenType::EQUAL);
  REQUIRE(lexer.isAtEnd());
}

TEST_CASE("parseEqualEqual") {
  std::string input = "==";
  LlamaLexer lexer(input);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(0).Type == LlamaTokenType::EQUAL_EQUAL);
  REQUIRE(lexer.getTokens().at(0).length() == 2);
  REQUIRE(lexer.getTokens().at(0).Lexeme == "==");
}

TEST_CASE("parseGreaterThan") {
  std::string input = ">";
  LlamaLexer lexer(input);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(0).Type == LlamaTokenType::GREATER_THAN);
  REQUIRE(lexer.isAtEnd());
}

TEST_CASE("parseGreaterThanEqual") {
  std::string input = ">=";
  LlamaLexer lexer(input);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(0).Type == LlamaTokenType::GREATER_THAN_EQUAL);
  REQUIRE(lexer.isAtEnd());
}

TEST_CASE("parseAlphaNumUnderscore") {
  std::string input = "not_a_keyword";
  LlamaLexer lexer(input);
  lexer.parseIdentifier({0,0});
  REQUIRE(lexer.getTokens().at(0).Type == LlamaTokenType::IDENTIFIER);
}

TEST_CASE("parseNumber") {
  std::string input = "123456789";
  LlamaLexer lexer(input);
  lexer.parseNumber({0,0});
  std::vector<Token> tokens = lexer.getTokens();
  REQUIRE(tokens.size() == 1);
  REQUIRE(tokens[0].Type == LlamaTokenType::NUMBER);
}

TEST_CASE("scanTokens") {
  std::string input = "{ }";
  LlamaLexer lexer(input);
  lexer.scanTokens();
  REQUIRE(lexer.getTokens().size() == 3);
  REQUIRE(lexer.getTokens()[0].Type == LlamaTokenType::OPEN_BRACE);
  REQUIRE(lexer.getTokens()[1].Type == LlamaTokenType::CLOSE_BRACE);
  REQUIRE(lexer.getTokens()[2].Type == LlamaTokenType::END_OF_FILE);
}

TEST_CASE("scanTokensParseIdentifierKeyword") {
  std::string input = "rule";
  LlamaLexer lexer(input);
  lexer.scanTokens();
  REQUIRE(lexer.getTokens().size() == 2);
  REQUIRE(lexer.getTokens()[0].Type == LlamaTokenType::RULE);
  REQUIRE(lexer.getTokens()[1].Type == LlamaTokenType::END_OF_FILE);
}

TEST_CASE("parseTokensParseIdentifierNonKeyword") {
  std::string input = "foobar";
  LlamaLexer lexer(input);
  lexer.scanTokens();
  REQUIRE(lexer.getTokens().size() == 2);
  REQUIRE(lexer.getTokens()[0].Type == LlamaTokenType::IDENTIFIER);
  REQUIRE(lexer.getTokens()[1].Type == LlamaTokenType::END_OF_FILE);
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
  REQUIRE(tokens[0].Type == LlamaTokenType::RULE);
  REQUIRE(tokens[1].Type == LlamaTokenType::IDENTIFIER);
  REQUIRE(tokens[2].Type == LlamaTokenType::OPEN_BRACE);
  REQUIRE(tokens[3].Type == LlamaTokenType::META);
  REQUIRE(tokens[4].Type == LlamaTokenType::COLON);
  REQUIRE(tokens[5].Type == LlamaTokenType::IDENTIFIER);
  REQUIRE(tokens[6].Type == LlamaTokenType::EQUAL);
  REQUIRE(tokens[7].Type == LlamaTokenType::DOUBLE_QUOTED_STRING);
  REQUIRE(tokens[8].Type == LlamaTokenType::IDENTIFIER);
  REQUIRE(tokens[9].Type == LlamaTokenType::EQUAL);
  REQUIRE(tokens[10].Type == LlamaTokenType::NUMBER);
  REQUIRE(tokens[11].Type == LlamaTokenType::CLOSE_BRACE);
  REQUIRE(tokens[12].Type == LlamaTokenType::END_OF_FILE);
  REQUIRE(lexer.getRuleCount() == 1);
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

TEST_CASE("parseIdentifierStringNumberLineCol") {
  std::string input = "description = \"this is my rule\"\nsomething = 56789 encodings=ASCII,UTF-8";
  LlamaLexer lexer(input);
  lexer.scanTokens();
  REQUIRE(lexer.getTokens().at(0).Pos.LineNum == 1);
  REQUIRE(lexer.getTokens().at(0).Pos.ColNum == 1);
  REQUIRE(lexer.getTokens().at(1).Pos.LineNum == 1);
  REQUIRE(lexer.getTokens().at(1).Pos.ColNum == 13);
  REQUIRE(lexer.getTokens().at(2).Pos.LineNum == 1);
  REQUIRE(lexer.getTokens().at(2).Pos.ColNum == 15);
  REQUIRE(lexer.getTokens().at(3).Pos.LineNum == 2);
  REQUIRE(lexer.getTokens().at(3).Pos.ColNum == 1);
  REQUIRE(lexer.getTokens().at(4).Pos.LineNum == 2);
  REQUIRE(lexer.getTokens().at(4).Pos.ColNum == 11);
  REQUIRE(lexer.getTokens().at(5).Pos.LineNum == 2);
  REQUIRE(lexer.getTokens().at(5).Pos.ColNum == 13);
  REQUIRE(lexer.getTokens().at(6).Pos.LineNum == 2);
  REQUIRE(lexer.getTokens().at(6).Pos.ColNum == 19);
  REQUIRE(lexer.getTokens().at(7).Pos.LineNum == 2);
  REQUIRE(lexer.getTokens().at(7).Pos.ColNum == 28);
  REQUIRE(lexer.getTokens().at(8).Pos.LineNum == 2);
  REQUIRE(lexer.getTokens().at(8).Pos.ColNum == 29);
}

TEST_CASE("lone!ThrowsException") {
  std::string input = "!";
  LlamaLexer lexer(input);
  REQUIRE_THROWS_AS(lexer.scanToken(), UnexpectedInputError);
}

TEST_CASE("andKeyword") {
  std::string input = "and";
  LlamaLexer lexer(input);
  lexer.parseIdentifier({0,0});
  REQUIRE(lexer.getTokens().at(0).Type == LlamaTokenType::AND);
}

TEST_CASE("orKeyword") {
  std::string input = "or";
  LlamaLexer lexer(input);
  lexer.parseIdentifier({0,0});
  REQUIRE(lexer.getTokens().at(0).Type == LlamaTokenType::OR);
}

TEST_CASE("parseSingleLineCommentIsIgnored") {
  std::string input = "this is a comment";
  LlamaLexer lexer(input);
  lexer.parseSingleLineComment();
  REQUIRE(lexer.isAtEnd());
}

TEST_CASE("parseSingleLineComment") {
  std::string input = "//this is a comment";
  LlamaLexer lexer(input);
  lexer.scanTokens();
  REQUIRE(lexer.isAtEnd());
}

TEST_CASE("parseMultiLineCommentIsIgnored") {
  std::string input = "this is a\nmulti-line comment */";
  LlamaLexer lexer(input);
  lexer.parseMultiLineComment({0,0});
  REQUIRE(lexer.isAtEnd());
}

TEST_CASE("parseMultiLineCommentThrowsIfUnterminated") {
  std::string input = "this is a\nmulti-line comment";
  LlamaLexer lexer(input);
  REQUIRE_THROWS_AS(lexer.parseMultiLineComment({0,0}), UnexpectedInputError);
}

TEST_CASE("parseRuleWithMultiLineComment") {
  std::string input = "rule /* this is a multi \n line comment */ MyRule {}";
  LlamaLexer lexer(input);
  lexer.scanTokens();
  REQUIRE(lexer.getTokens().size() == 5);
}

TEST_CASE("peekWhenSizeIsZeroShouldReturnNullChar") {
  std::string input = "";
  LlamaLexer lexer(input);
  REQUIRE(lexer.peek() == '\0');
}

TEST_CASE("peekWhenNextToLastShouldReturnNullChar") {
  std::string input = "a";
  LlamaLexer lexer(input);
  REQUIRE(lexer.peek() == '\0');
}

TEST_CASE("peekWhenAtEndShouldReturnNullChar") {
  std::string input = "a";
  LlamaLexer lexer(input);
  lexer.advance();
  REQUIRE(lexer.peek() == '\0');
}

TEST_CASE("parseSingleLineCommentWithinMultiLineComment") {
  std::string input = "/* this \nis a // single line comment */";
  LlamaLexer lexer(input);
  lexer.scanTokens();
  REQUIRE(lexer.getTokens().size() == 1);
}

TEST_CASE("parseMultiLineCommentWithManyAsterisks") {
  std::string input = "/******************* this is a multi-line \ncomment *******************/";
  LlamaLexer lexer(input);
  lexer.scanTokens();
  REQUIRE(lexer.getTokens().size() == 1);
}

TEST_CASE("parseMultiLineCommentUnterminatedComplex") {
  std::string input = "/* /// ***** /*  //";
  LlamaLexer lexer(input);
  REQUIRE_THROWS_AS(lexer.scanTokens(), UnexpectedInputError);
}

TEST_CASE("parseSingleLineCommentWithMultiLineCommentIsIgnored") {
  std::string input = "// this is a single line comment /* this is a multi-line comment*/";
  LlamaLexer lexer(input);
  lexer.scanTokens();
  REQUIRE(lexer.getTokens().size() == 1);
}

TEST_CASE("parseSingleLineCommentWithMultiLineCommentWithNewlineThrows") {
  std::string input = "// this is a single line comment /* this is a multi-line comment with a newline \n*/";
  LlamaLexer lexer(input);
  REQUIRE_THROWS_AS(lexer.scanTokens(), UnexpectedInputError);
}

TEST_CASE("parseMultiLineCommentIncreasesLineNumAndResetsColumnNum") {
  std::string input = "/* this is a multi-line comment\n\n\n*/";
  LlamaLexer lexer(input);
  lexer.scanTokens();
  REQUIRE(lexer.getTokens().size() == 1);
  REQUIRE(lexer.Pos.LineNum == 4);
  REQUIRE(lexer.Pos.ColNum == 3);
}

TEST_CASE("parseStringWithEscapedDoubleQuote") {
  std::string input = R"("this is a \"string\" that is escaped")";
  LlamaLexer lexer(input);
  lexer.scanTokens();
  REQUIRE(lexer.getTokens().size() == 2);
  REQUIRE(lexer.getTokens().at(0).Type == LlamaTokenType::DOUBLE_QUOTED_STRING);
  REQUIRE(lexer.getTokens().at(0).length() == input.size() - 2);
  REQUIRE(lexer.getTokens().at(1).Type == LlamaTokenType::END_OF_FILE);
}

TEST_CASE("multipleRuleCount") {
  std::string input = "rule rule rule rule rule rule";
  LlamaLexer lexer(input);
  lexer.scanTokens();
  REQUIRE(lexer.getRuleCount() == 6);
  REQUIRE(lexer.getRuleIndices() == std::vector<size_t>{0, 1, 2, 3, 4, 5});
}