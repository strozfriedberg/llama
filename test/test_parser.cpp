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
  SFHASH_HashAlgorithm hash;
  REQUIRE_NOTHROW(hash = parser.parseHash());
  REQUIRE(hash == SFHASH_MD5);
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
  HashExpr expr;
  REQUIRE_NOTHROW(expr = parser.parseHashExpr());
  REQUIRE(expr.Alg == SFHASH_MD5);
  REQUIRE(expr.Val == "test");
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

TEST_CASE("parseHashSectionMultipleAlg") {
  std::string input = "hash: md5 = \"test\"\nsha1 = \"abcdef\"";
  LlamaParser parser(input, getTokensFromString(input));
  HashSection hashSection;
  REQUIRE_NOTHROW(hashSection = parser.parseHashSection());
  REQUIRE(hashSection.Hashes.at(0).Alg == SFHASH_MD5);
  REQUIRE(hashSection.Hashes.at(0).Val == "test");
  REQUIRE(hashSection.Hashes.at(1).Alg == SFHASH_SHA_1);
  REQUIRE(hashSection.Hashes.at(1).Val == "abcdef");
  REQUIRE(hashSection.HashAlgs == (SFHASH_MD5 | SFHASH_SHA_1));
}

TEST_CASE("parseOperatorThrowsIfNotOperator") {
  std::string input = "notAnOperator";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseOperator(), ParserError);
}

TEST_CASE("parseOperatorDoesNotThrowIfOperator") {
  std::string input = "==";
  LlamaParser parser(input, getTokensFromString(input));
  TokenType op;
  REQUIRE_NOTHROW(op = parser.parseOperator());
  REQUIRE(op == TokenType::EQUAL_EQUAL);
}

TEST_CASE("parseStringModDoesNotThrowIfStringMod") {
  std::string input = "= \"test\" nocase";
  LlamaParser parser(input, getTokensFromString(input));
  std::vector<PatternDef> defs;
  REQUIRE_NOTHROW(defs = parser.parsePatternDef());
  REQUIRE(defs.size() == 1);
  REQUIRE(defs.at(0).Pattern == "test");
  REQUIRE(defs.at(0).Encoding == lg_get_encoding_id("ASCII"));
  REQUIRE(defs.at(0).Options.CaseInsensitive);
  REQUIRE(!defs.at(0).Options.FixedString);
}

TEST_CASE("parseEncodingsThrowsIfNotEqualSign") {
  std::string input = "notEqualSign";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseEncodings(), ParserError);
}

TEST_CASE("parseEncodingsIfNoEncodingAfterEqualSign") {
  std::string input = "encodings=";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseEncodings(), ParserError);
}

TEST_CASE("parseEncodingsIfDanglingComma") {
  std::string input = "encodings=UTF-8,";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseEncodings(), ParserError);
}

TEST_CASE("parseEncodingsDoesNotThrowIfEncodings") {
  std::string input = "=UTF-8,UTF-16LE";
  LlamaParser parser(input, getTokensFromString(input));
  std::vector<int> encodings;
  REQUIRE_NOTHROW(encodings = parser.parseEncodings());
  REQUIRE(encodings.size() == 2);
  REQUIRE(encodings.at(0) == 0);
  REQUIRE(encodings.at(1) == 2);
}

TEST_CASE("parseStringDefThrowsIfNotStringDef") {
  std::string input = "rule";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parsePatternDef(), ParserError);
}

TEST_CASE("parseStringDefDoesNotThrowIfStringDef") {
  std::string input = "= \"test\" encodings=UTF-8 nocase fixed";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parsePatternDef());
}

TEST_CASE("parsePatternsSectionThrowsIfNotPatterns") {
  std::string input = "rule";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parsePatternsSection(), ParserError);
}

TEST_CASE("parsePatternsSectionDoesNotThrowIfPatterns") {
  std::string input = R"(patterns:
  a = "test" encodings=UTF-8 nocase fixed
  b = "test2" encodings=UTF-8 nocase fixed
  c = { 12 34 56 78 9a bc de f0 }
  )";
  LlamaParser parser(input, getTokensFromString(input));
  PatternSection patternSection;
  REQUIRE_NOTHROW(patternSection = parser.parsePatternsSection());
  REQUIRE(patternSection.Patterns.size() == 3);
  REQUIRE(patternSection.Patterns.find("a")->second.at(0).Pattern == "test");
  REQUIRE(patternSection.Patterns.find("b")->second.at(0).Pattern == "test2");
  REQUIRE(patternSection.Patterns.find("c")->second.at(0).Pattern == "\\z12\\z34\\z56\\z78\\z9a\\zbc\\zde\\zf0");
}

TEST_CASE("parseTermWithAnd") {
  std::string input = "any(s1, s2, s3) and count(s1, 5) == 5";
  LlamaParser parser(input, getTokensFromString(input));
  std::shared_ptr<Node> node;
  REQUIRE_NOTHROW(node = parser.parseTerm());
  REQUIRE(node->Type == NodeType::AND);
  REQUIRE(node->Left->Type == NodeType::FUNC);
  REQUIRE(std::get<ConditionFunction>(node->Left->Value).Name == TokenType::ANY);
  REQUIRE(node->Right->Type == NodeType::FUNC);
  REQUIRE(std::get<ConditionFunction>(node->Right->Value).Name == TokenType::COUNT);
}

TEST_CASE("parseTermWithoutAnd") {
  std::string input = "any(s1, s2, s3)";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseTerm());
}

TEST_CASE("parseExpr") {
  std::string input = "(any(s1, s2, s3) and count(s1, 5) == 5) or all(s1, s2, s3)";
  LlamaParser parser(input, getTokensFromString(input));
  auto node = std::make_shared<Node>();
  REQUIRE_NOTHROW(node = parser.parseExpr());
  REQUIRE(node->Type == NodeType::OR);
  REQUIRE(node->Left->Type == NodeType::AND);
  REQUIRE(node->Right->Type == NodeType::FUNC);
}

TEST_CASE("parseConditionSection") {
  std::string input = "condition:\n  (any(s1, s2, s3) and count(s1, 5) == 5) or all(s1, s2, s3)";
  LlamaParser parser(input, getTokensFromString(input));
  ConditionSection section;
  REQUIRE_NOTHROW(section = parser.parseConditionSection());
  REQUIRE(parser.CurIdx == parser.Tokens.size() - 1);
  REQUIRE(section.Tree->Type == NodeType::OR);
}

TEST_CASE("parseSignatureSection") {
  std::string input = "signature:\n \"EXE\"\n\"MUI\"";
  LlamaParser parser(input, getTokensFromString(input));
  SignatureSection section;
  REQUIRE_NOTHROW(section = parser.parseSignatureSection());
  REQUIRE(section.Signatures.size() == 2);
  REQUIRE(section.Signatures.at(0) == "EXE");
  REQUIRE(section.Signatures.at(1) == "MUI");
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
  GrepSection section;
  REQUIRE_NOTHROW(section = parser.parseGrepSection());
  REQUIRE(section.Patterns.Patterns.size() == 2);
  REQUIRE(section.Patterns.Patterns.find("a")->second.at(0).Pattern == "test");
  REQUIRE(section.Patterns.Patterns.find("a")->second.at(0).Encoding == lg_get_encoding_id("UTF-8"));
  REQUIRE(section.Condition.Tree->Type == NodeType::AND);
  REQUIRE(section.Condition.Tree->Left->Type == NodeType::FUNC);
  REQUIRE(section.Condition.Tree->Right->Type == NodeType::FUNC);
}

TEST_CASE("parseFileMetadataDefFileSize") {
  std::string input = "filesize > 100";
  LlamaParser parser(input, getTokensFromString(input));
  FileMetadataDef def;
  REQUIRE_NOTHROW(def = parser.parseFileMetadataDef());
  REQUIRE(def.Property == TokenType::FILESIZE);
  REQUIRE(def.Operator == TokenType::GREATER_THAN);
  REQUIRE(def.Value == "100");
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
  MetaSection meta;
  REQUIRE_NOTHROW(meta = parser.parseMetaSection());
  REQUIRE(meta.Fields.size() == 2);
  REQUIRE(meta.Fields.find("arbitrary")->second == "something");
  REQUIRE(meta.Fields.find("another")->second == "something else");
}

TEST_CASE("parseRuleDecl") {
  std::string input = R"(
  rule MyRule {
    meta:
      description = "test"
    hash:
      md5 = "abcdef"
    signature:
      "EXE"
    file_metadata:
      created > "2023-05-04"
      modified < "2023-05-06"
      filesize >= 100
  }
  )";
  LlamaParser parser(input, getTokensFromString(input));
  Rule rule;
  REQUIRE_NOTHROW(rule = parser.parseRuleDecl());
  REQUIRE(rule.Hash.Hashes.size() == 1);
  REQUIRE(rule.Signature.Signatures.size() == 1);
}

TEST_CASE("parseRuleDeclThrowsIfSectionsAreOutOfOrder") {
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
        c = { 34 56 78 ab cd EF }
      condition:
        any(a, b) and count(a) == 5
  }
  )";
  LlamaParser parser(input, getTokensFromString(input));
  std::vector<Rule> rules;
  REQUIRE_NOTHROW(rules = parser.parseRules());
  REQUIRE(rules.size() == 2);
  REQUIRE(rules.at(0).Name == "MyRule");
  REQUIRE(rules.at(0).Meta.Fields.find("description")->second == "test");
  REQUIRE(rules.at(1).Name == "AnotherRule");
  REQUIRE(rules.at(1).Grep.Patterns.Patterns.find("c")->second.at(0).Pattern == "\\z34\\z56\\z78\\zab\\zcd\\zEF");
}

TEST_CASE("parseHexString") {
  std::string input = "34 56 78 9f }";
  LlamaParser parser(input, getTokensFromString(input));
  std::vector<PatternDef> defs;
  REQUIRE_NOTHROW(defs = parser.parseHexString());
  REQUIRE(defs.at(0).Pattern == "\\z34\\z56\\z78\\z9f");
}

TEST_CASE("parseHexStringThrowsIfUnterminated") {
  std::string input = "34 56 78 9f";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseHexString(), ParserError);
}

TEST_CASE("parseHexStringThrowsIfInvalidHex") {
  std::string input = "34 56 78 9z }";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseHexString(), ParserError);
}

TEST_CASE("parseHexStringThrowsIfNotTwoByteDigits") {
  std::string input = "5 }";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseHexString(), ParserError);
}

TEST_CASE("parseHexStringThrowsIfNotNumberOrIdentifier") {
  std::string input = "(8) }";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseHexString(), ParserError);
}

TEST_CASE("parseHexStringThrowIfEmpty") {
  std::string input = "}";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseHexString(), ParserError);
}

TEST_CASE("parserParseNumber") {
  std::string input = "123456";
  LlamaParser parser(input, getTokensFromString(input));
  std::string num = parser.parseNumber();
  REQUIRE(num == "123456");
}

TEST_CASE("parseFuncCallAny") {
  std::string input = "any(s1, s2, s3)";
  LlamaParser parser(input, getTokensFromString(input));
  ConditionFunction func;
  REQUIRE_NOTHROW(func = parser.parseFuncCall());
  REQUIRE(func.Name == TokenType::ANY);
  REQUIRE(func.Args.size() == 3);
  REQUIRE(func.Args.at(0) == "s1");
  REQUIRE(func.Args.at(1) == "s2");
  REQUIRE(func.Args.at(2) == "s3");
}

TEST_CASE("parseFuncCallAll") {
  std::string input = "all()";
  LlamaParser parser(input, getTokensFromString(input));
  ConditionFunction func;
  REQUIRE_NOTHROW(func = parser.parseFuncCall());
  REQUIRE(func.Name == TokenType::ALL);
  REQUIRE(func.Args.size() == 0);
}

TEST_CASE("parseFuncCallWithNumber") {
  std::string input = "count(s1, 5)";
  LlamaParser parser(input, getTokensFromString(input));
  ConditionFunction func;
  REQUIRE_NOTHROW(func = parser.parseFuncCall());
  REQUIRE(func.Name == TokenType::COUNT);
  REQUIRE(func.Args.size() == 2);
  REQUIRE(func.Args.at(0) == "s1");
  REQUIRE(func.Args.at(1) == "5");
}

TEST_CASE("parseFuncCallWithOperator") {
  std::string input = "offset(s1, 5) == 5";
  LlamaParser parser(input, getTokensFromString(input));
  ConditionFunction func;
  REQUIRE_NOTHROW(func = parser.parseFuncCall());
  REQUIRE(func.Name == TokenType::OFFSET);
  REQUIRE(func.Args.size() == 2);
  REQUIRE(func.Args.at(0) == "s1");
  REQUIRE(func.Args.at(1) == "5");
  REQUIRE(func.Value == "5");
}

TEST_CASE("parseFactorProducesFuncNodeIfNoParen") {
  std::string input = "any(s1, s2, s3)";
  LlamaParser parser(input, getTokensFromString(input));
  std::shared_ptr<Node> node;
  REQUIRE_NOTHROW(node = parser.parseFactor());
  REQUIRE(node->Type == NodeType::FUNC);
  REQUIRE(std::get<ConditionFunction>(node->Value).Name == TokenType::ANY);
  REQUIRE(std::get<ConditionFunction>(node->Value).Args.size() == 3);
}