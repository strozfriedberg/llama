#include <catch2/catch_test_macros.hpp>

#include "lexer.h"
#include "parser.h"
#include "rulereader.h"

LlamaLexer getLexer(const std::string& input) {
  LlamaLexer lexer(input);
  lexer.scanTokens();
  lexer.getTokens();
  return lexer;
}

Token makeToken(LlamaTokenType type) {
  return Token(type, "" ,{0, 0});
}

TEST_CASE("LlamaParser") {
  std::vector<Token> tokens;
  std::string input = "";
  LlamaParser parser(input, tokens);
}

TEST_CASE("TestLlamaParserPrevious") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(input, getLexer(input).getTokens());
  parser.CurIdx = 1;
  REQUIRE(parser.previous().Type == LlamaTokenType::RULE);
}

TEST_CASE("TestLlamaParserPeek") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(input, getLexer(input).getTokens());
  parser.CurIdx = 1;
  REQUIRE(parser.peek().Type == LlamaTokenType::OPEN_BRACE);
}

TEST_CASE("TestLlamaParserIsAtEndTrue") {
  std::string input = "rule";
  LlamaParser parser(input, getLexer(input).getTokens());
  parser.advance();
  REQUIRE(parser.isAtEnd());
}

TEST_CASE("TestLlamaParserIsAtEndFalse") {
  std::string input = "rule";
  LlamaParser parser(input, getLexer(input).getTokens());
  REQUIRE_FALSE(parser.isAtEnd());
}

TEST_CASE("TestLlamaParserAdvance") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(input, getLexer(input).getTokens());
  parser.advance();
  REQUIRE(parser.previous().Type == LlamaTokenType::RULE);
  REQUIRE(parser.peek().Type == LlamaTokenType::OPEN_BRACE);
}

TEST_CASE("TestLlamaParserCheckTrue") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(input, getLexer(input).getTokens());
  REQUIRE(parser.checkAny(LlamaTokenType::RULE));
}

TEST_CASE("TestLlamaParserCheckFalse") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(input, getLexer(input).getTokens());
  REQUIRE_FALSE(parser.checkAny(LlamaTokenType::OPEN_BRACE));
}

TEST_CASE("TestLlamaParserMatchTrue") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(input, getLexer(input).getTokens());
  REQUIRE(parser.matchAny(LlamaTokenType::RULE));
  REQUIRE(parser.CurIdx == 1);
}

TEST_CASE("TestLlamaParserMatchFalse") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(input, getLexer(input).getTokens());
  REQUIRE_FALSE(parser.matchAny(LlamaTokenType::OPEN_BRACE));
  REQUIRE(parser.CurIdx == 0);
}

TEST_CASE("TestLlamaParserMatchMultipleTrue") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(input, getLexer(input).getTokens());
  REQUIRE(parser.matchAny(LlamaTokenType::RULE, LlamaTokenType::META));
}

TEST_CASE("TestLlamaParserMatchMultipleFalse") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(input, getLexer(input).getTokens());
  REQUIRE_FALSE(parser.matchAny(LlamaTokenType::OPEN_BRACE, LlamaTokenType::META));
}

TEST_CASE("parseHashThrowsIfNotHash") {
  std::string input = "notAHash";
  LlamaParser parser(input, getLexer(input).getTokens());
  REQUIRE_THROWS_AS(parser.parseHash(), ParserError);
}

TEST_CASE("parseHashDoesNotThrowIfHash") {
  std::string input = "md5";
  LlamaParser parser(input, getLexer(input).getTokens());
  SFHASH_HashAlgorithm hash;
  REQUIRE_NOTHROW(hash = parser.parseHash());
  REQUIRE(hash == SFHASH_MD5);
}

TEST_CASE("parseFileHashRecordThrowsIfNotEqualityOperator") {
  std::string input = "md5 \"test\"";
  LlamaParser parser(input, getLexer(input).getTokens());
  REQUIRE_THROWS_AS(parser.parseFileHashRecord(), ParserError);
}

TEST_CASE("parseFileHashRecordThrowsIfNotString") {
  std::string input = "md5 == 1234";
  LlamaParser parser(input, getLexer(input).getTokens());
  REQUIRE_THROWS_AS(parser.parseFileHashRecord(), ParserError);
}

TEST_CASE("parseFileHashRecordDoesNotThrowIfHashAndString") {
  std::string input = "md5 == \"test\"";
  LlamaParser parser(input, getLexer(input).getTokens());
  REQUIRE_NOTHROW(parser.parseFileHashRecord());
}

TEST_CASE("parseHashSectionThrowsIfNotAHash") {
  std::string input = "notAHash";
  LlamaParser parser(input, getLexer(input).getTokens());
  REQUIRE_THROWS_AS(parser.parseHashSection(), ParserError);
}

TEST_CASE("parseHashSectionDoesNotThrowIfHash") {
  std::string input = "md5 == \"test\"";
  LlamaParser parser(input, getLexer(input).getTokens());
  REQUIRE_NOTHROW(parser.parseHashSection());
}

TEST_CASE("parseHashSectionMultipleAlg") {
  std::string input = "md5 == \"test\"\nsha1 == \"abcdef\"";
  LlamaParser parser(input, getLexer(input).getTokens());
  HashSection hashSection;
  REQUIRE_NOTHROW(hashSection = parser.parseHashSection());
  auto it = findKey(hashSection.FileHashRecords.at(0), SFHASH_MD5);
  REQUIRE(it->second == "\"test\"");
  it = findKey(hashSection.FileHashRecords.at(1), SFHASH_SHA_1);
  REQUIRE(it->second == "\"abcdef\"");
}

TEST_CASE("parseHashSectionMultipleRecords") {
  std::string input = "md5 == \"test\", sha1 == \"abcdef\"\nmd5 == \"test2\"";
  LlamaParser parser(input, getLexer(input).getTokens());
  HashSection hashSection;
  REQUIRE_NOTHROW(hashSection = parser.parseHashSection());
  REQUIRE(hashSection.FileHashRecords.size() == 2);
  REQUIRE(findKey(hashSection.FileHashRecords.at(0), SFHASH_MD5)->second == "\"test\"");
  REQUIRE(findKey(hashSection.FileHashRecords.at(0), SFHASH_SHA_1)->second == "\"abcdef\"");
  REQUIRE(findKey(hashSection.FileHashRecords.at(1), SFHASH_MD5)->second == "\"test2\"");
  REQUIRE(hashSection.HashAlgs == (SFHASH_MD5 | SFHASH_SHA_1));
}

TEST_CASE("parseOperatorThrowsIfNotOperator") {
  std::string input = "notAnOperator";
  LlamaParser parser(input, getLexer(input).getTokens());
  REQUIRE_THROWS_AS(parser.parseOperator(), ParserError);
}

TEST_CASE("parseOperatorDoesNotThrowIfOperator") {
  std::string input = "==";
  LlamaParser parser(input, getLexer(input).getTokens());
  REQUIRE_NOTHROW(parser.parseOperator());
}

TEST_CASE("parseStringModDoesNotThrowIfStringMod") {
  std::string input = "= \"test\" nocase";
  LlamaParser parser(input, getLexer(input).getTokens());
  PatternDef def;
  REQUIRE_NOTHROW(def = parser.parsePatternDef());
  REQUIRE(def.Pattern == "\"test\"");
  REQUIRE(def.Enc == Encodings{0,0});
  REQUIRE(def.Options.CaseInsensitive);
  REQUIRE(!def.Options.FixedString);
}

TEST_CASE("parseEncodingsThrowsIfNotEqualSign") {
  std::string input = "notEqualSign";
  LlamaParser parser(input, getLexer(input).getTokens());
  REQUIRE_THROWS_AS(parser.parseEncodings(), ParserError);
}

TEST_CASE("parseEncodingsIfNoEncodingAfterEqualSign") {
  std::string input = "encodings=";
  LlamaParser parser(input, getLexer(input).getTokens());
  REQUIRE_THROWS_AS(parser.parseEncodings(), ParserError);
}

TEST_CASE("parseEncodingsIfDanglingComma") {
  std::string input = "encodings=UTF-8,";
  LlamaParser parser(input, getLexer(input).getTokens());
  REQUIRE_THROWS_AS(parser.parseEncodings(), ParserError);
}

TEST_CASE("parseEncodingsThrowsIfNonIdentifierBetweenCommas") {
  std::string input = "encodings=UTF-8,=,UTF-16";
  LlamaParser parser(input, getLexer(input).getTokens());
  REQUIRE_THROWS_AS(parser.parseEncodings(), ParserError);
}

TEST_CASE("parseEncodingsThrowsIfNonIdentifierIsLastElement") {
  std::string input = "encodings=UTF-8,UTF-16,=";
  LlamaParser parser(input, getLexer(input).getTokens());
  REQUIRE_THROWS_AS(parser.parseEncodings(), ParserError);
}

TEST_CASE("parseEncodingsDoesNotThrowIfEncodings") {
  std::string input = "=UTF-8,UTF-16LE";
  LlamaParser parser(input, getLexer(input).getTokens());
  Encodings encodings;
  REQUIRE_NOTHROW(encodings = parser.parseEncodings());
  REQUIRE(encodings.first == 1);
  REQUIRE(encodings.second == 4);
}

TEST_CASE("parseStringDefThrowsIfNotStringDef") {
  std::string input = "rule";
  LlamaParser parser(input, getLexer(input).getTokens());
  REQUIRE_THROWS_AS(parser.parsePatternDef(), ParserError);
}

TEST_CASE("parseRuleThrowsIfWrongOrderPatternMods") {
  std::string input = "rule { patterns: a = \"test\" encodings=UTF-8 fixed nocase }";
  LlamaParser parser(input, getLexer(input).getTokens());
  REQUIRE_THROWS(parser.parseRuleDecl());
}

TEST_CASE("parsePatternsSectionThrowsIfNotIdentifier") {
  std::string input = "rule";
  LlamaParser parser(input, getLexer(input).getTokens());
  REQUIRE_THROWS_AS(parser.parsePatternsSection(), ParserError);
}

TEST_CASE("parsePatternsSectionDoesNotThrowIfPatterns") {
  std::string input = R"(
  a = "test" fixed nocase encodings=UTF-8
  b = "test2" fixed nocase encodings=UTF-8
  c = { 12 34 56 78 9a bc de f0 }
  )";
  LlamaParser parser(input, getLexer(input).getTokens());
  PatternSection patternSection;
  REQUIRE_NOTHROW(patternSection = parser.parsePatternsSection());
  REQUIRE(patternSection.Patterns.size() == 3);
  REQUIRE(patternSection.Patterns.find("a")->second.Pattern == "\"test\"");
  REQUIRE(patternSection.Patterns.find("b")->second.Pattern == "\"test2\"");
  REQUIRE(patternSection.Patterns.find("c")->second.Pattern == "\\z12\\z34\\z56\\z78\\z9a\\zbc\\zde\\zf0");
}

TEST_CASE("parseTermWithAnd") {
  std::string input = "any(s1, s2, s3) and count(s1) == 5";
  LlamaParser parser(input, getLexer(input).getTokens());
  std::shared_ptr<Node> node = std::make_shared<BoolNode>();
  REQUIRE_NOTHROW(node = parser.parseTerm(LlamaTokenType::CONDITION));
  REQUIRE(node->Type == NodeType::AND);
  auto left = std::static_pointer_cast<FuncNode>(node->Left);
  REQUIRE(node->Left->Type == NodeType::FUNC);
  REQUIRE(left->Value.Name == "any");
  auto right = std::static_pointer_cast<FuncNode>(node->Right);
  REQUIRE(node->Right->Type == NodeType::FUNC);
  REQUIRE(right->Value.Name == "count");
}

TEST_CASE("parseTermWithoutAnd") {
  std::string input = "any()";
  LlamaParser parser(input, getLexer(input).getTokens());
  REQUIRE_NOTHROW(parser.parseTerm(LlamaTokenType::CONDITION));
}

TEST_CASE("parseExpr") {
  std::string input = "(any(s1, s2, s3) and length(s1, 5) == 5) or all()";
  LlamaParser parser(input, getLexer(input).getTokens());
  std::shared_ptr<Node> node = std::make_shared<BoolNode>();
  REQUIRE_NOTHROW(node = parser.parseExpr(LlamaTokenType::CONDITION));
  REQUIRE(node->Type == NodeType::OR);
  REQUIRE(node->Left);
  REQUIRE(node->Left->Type == NodeType::AND);
  REQUIRE(node->Right);
  REQUIRE(node->Right->Type == NodeType::FUNC);
}

TEST_CASE("parseConditionSection") {
  std::string input = "(any(s1, s2, s3) and count(s1) == 5) or all(s1, s2, s3)";
  LlamaParser parser(input, getLexer(input).getTokens());
  std::shared_ptr<Node> node;
  REQUIRE_NOTHROW(node = parser.parseExpr(LlamaTokenType::CONDITION));
  REQUIRE(parser.CurIdx == parser.Tokens.size() - 1);
  REQUIRE(node->Type == NodeType::OR);
}

TEST_CASE("parseSignatureSection") {
  std::string input = "name == \"Executable\" or id == \"123456789\"";
  LlamaParser parser(input, getLexer(input).getTokens());
  std::shared_ptr<Node> node;
  REQUIRE_NOTHROW(node = parser.parseExpr(LlamaTokenType::SIGNATURE));
  REQUIRE(node->Type == NodeType::OR);
  auto propNodeLeft = std::static_pointer_cast<PropertyNode>(node->Left);
  REQUIRE(node->Left->Type == NodeType::PROP);
  REQUIRE(parser.getLexemeAt(propNodeLeft->Value.Name) == "name");
  REQUIRE(parser.getLexemeAt(propNodeLeft->Value.Val) == "\"Executable\"");
  auto propNodeRight = std::static_pointer_cast<PropertyNode>(node->Right);
  REQUIRE(node->Right->Type == NodeType::PROP);
  REQUIRE(parser.getLexemeAt(propNodeRight->Value.Name) == "id");
  REQUIRE(parser.getLexemeAt(propNodeRight->Value.Val) == "\"123456789\"");
}

TEST_CASE("parseGrepSection") {
  std::string input = R"(
    patterns:
      a = "test" fixed nocase encodings=UTF-8
      b = "test2" fixed nocase encodings=UTF-8
    condition:
      any(a, b) and offset(a, 5) == 5
)";
  LlamaParser parser(input, getLexer(input).getTokens());
  GrepSection section;
  REQUIRE_NOTHROW(section = parser.parseGrepSection());
  REQUIRE(section.Patterns.Patterns.size() == 2);
  auto patDef = section.Patterns.Patterns.find("a")->second;
  REQUIRE(patDef.Pattern == "\"test\"");
  REQUIRE(patDef.Enc.first == 9);
  REQUIRE(patDef.Enc.second == 10);
  REQUIRE(section.Condition->Type == NodeType::AND);
  REQUIRE(section.Condition->Left->Type == NodeType::FUNC);
  REQUIRE(section.Condition->Right->Type == NodeType::FUNC);
}

TEST_CASE("parseFileMetadataSection") {
  std::string input = R"(created > "2023-05-04" or (modified < "2023-05-06" and filesize >= 100))";
  LlamaParser parser(input, getLexer(input).getTokens());
  REQUIRE_NOTHROW(parser.parseExpr(LlamaTokenType::FILE_METADATA));
}

TEST_CASE("parseMetaSection") {
  std::string input = R"(
    arbitrary = "something"
    another = "something else"
  )";
  LlamaParser parser(input, getLexer(input).getTokens());
  MetaSection meta;
  REQUIRE_NOTHROW(meta = parser.parseMetaSection());
  REQUIRE(meta.Fields.size() == 2);
  REQUIRE(meta.Fields.find("arbitrary")->second == "\"something\"");
  REQUIRE(meta.Fields.find("another")->second == "\"something else\"");
}

TEST_CASE("parseRuleDecl") {
  std::string input = R"(
  rule MyRule {
    meta:
      description = "test"
    hash:
      md5 == "abcdef"
    file_metadata:
      created > "2023-05-04" and modified < "2023-05-06"
    signature:
      name == "Executable"
  }
  )";
  LlamaParser parser(input, getLexer(input).getTokens());
  Rule rule;
  REQUIRE_NOTHROW(rule = parser.parseRuleDecl());
  REQUIRE(rule.Hash.FileHashRecords.size() == 1);
  auto root = std::static_pointer_cast<PropertyNode>(rule.Signature);
  REQUIRE(parser.getLexemeAt(root->Value.Name) == "name");
  REQUIRE(parser.getLexemeAt(root->Value.Val) == "\"Executable\"");
}

TEST_CASE("parseRuleDeclThrowsIfSectionsAreOutOfOrder") {
  std::string input = R"(
  rule MyRule {
    grep:
      patterns:
        a = "test" fixed nocase encodings=UTF-8
        b = "test2" fixed nocase encodings=UTF-8
      condition:
        any(a, b) and offset(a, 5) == 5
    hash: md5 == "test"
  }
  )";
  LlamaParser parser(input, getLexer(input).getTokens());
  REQUIRE_THROWS_AS(parser.parseRuleDecl(), ParserError);
}

TEST_CASE("startRule") {
  std::string input = R"(
  rule MyRule {
    meta:
      description = "test"
    signature:
      name == "Executable"
  }
  rule AnotherRule {
    meta:
      description = "test"
    grep:
      patterns:
        a = "test" fixed nocase encodings=UTF-8
        b = "test2" fixed nocase encodings=UTF-8
        c = { 34 56 78 ab cd EF }
      condition:
        any(a, b) and count(a) == 5
  }
  )";
  LlamaLexer lexer = getLexer(input);
  LlamaParser parser(input, lexer.getTokens());
  std::vector<Rule> rules;
  REQUIRE_NOTHROW(rules = parser.parseRules(lexer.getRuleCount(), lexer.getRuleIndices()));
  REQUIRE(rules.size() == 2);
  REQUIRE(rules.at(0).Name == "MyRule");
  REQUIRE(rules.at(0).Meta.Fields.find("description")->second == "\"test\"");
  REQUIRE(rules.at(1).Name == "AnotherRule");
  REQUIRE(rules.at(1).Grep.Patterns.Patterns.find("c")->second.Pattern == "\\z34\\z56\\z78\\zab\\zcd\\zEF");
}

TEST_CASE("parseHexString") {
  std::string input = "34 56 78 9f }";
  LlamaParser parser(input, getLexer(input).getTokens());
  PatternDef def;
  REQUIRE_NOTHROW(def = parser.parseHexString());
  REQUIRE(def.Pattern == "\\z34\\z56\\z78\\z9f");
}

TEST_CASE("parseHexStringThrowsIfUnterminated") {
  std::string input = "34 56 78 9f";
  LlamaParser parser(input, getLexer(input).getTokens());
  REQUIRE_THROWS_AS(parser.parseHexString(), ParserError);
}

TEST_CASE("parseHexStringThrowsIfInvalidHex") {
  std::string input = "34 56 78 9z }";
  LlamaParser parser(input, getLexer(input).getTokens());
  REQUIRE_THROWS_AS(parser.parseHexString(), ParserError);
}

TEST_CASE("parseHexStringThrowsIfNotTwoByteDigits") {
  std::string input = "5 }";
  LlamaParser parser(input, getLexer(input).getTokens());
  REQUIRE_THROWS_AS(parser.parseHexString(), ParserError);
}

TEST_CASE("parseHexStringThrowsIfNotNumberOrIdentifier") {
  std::string input = "(8) }";
  LlamaParser parser(input, getLexer(input).getTokens());
  REQUIRE_THROWS_AS(parser.parseHexString(), ParserError);
}

TEST_CASE("parseHexStringThrowIfEmpty") {
  std::string input = "}";
  LlamaParser parser(input, getLexer(input).getTokens());
  REQUIRE_THROWS_AS(parser.parseHexString(), ParserError);
}

TEST_CASE("parserExpectNumber") {
  std::string input = "123456";
  LlamaParser parser(input, getLexer(input).getTokens());
  REQUIRE_NOTHROW(parser.expect(LlamaTokenType::NUMBER));
}

TEST_CASE("parseFuncCallAny") {
  std::string input = "any(s1, s2, s3)";
  LlamaParser parser(input, getLexer(input).getTokens());
  FuncNode node;
  REQUIRE_NOTHROW(node = parser.parseFuncCall());
  REQUIRE(node.Value.Name == "any");
  REQUIRE(node.Value.Args.size() == 3);
  REQUIRE(node.Value.Args.at(0) == "s1");
  REQUIRE(node.Value.Args.at(1) == "s2");
  REQUIRE(node.Value.Args.at(2) == "s3");
}

TEST_CASE("parseFuncCallAll") {
  std::string input = "all()";
  LlamaParser parser(input, getLexer(input).getTokens());
  FuncNode node;
  REQUIRE_NOTHROW(node = parser.parseFuncCall());
  REQUIRE(node.Value.Name == "all");
  REQUIRE(node.Value.Args.size() == 0);
}

TEST_CASE("parseFuncCallWithNumber") {
  std::string input = "count(s1) == 5";
  LlamaParser parser(input, getLexer(input).getTokens());
  FuncNode node;
  REQUIRE_NOTHROW(node = parser.parseFuncCall());
  REQUIRE(node.Value.Name == "count");
  REQUIRE(node.Value.Args.size() == 1);
  REQUIRE(node.Value.Args.at(0) == "s1");
  REQUIRE(parser.getLexemeAt(node.Value.Operator) == "==");
  REQUIRE(parser.getLexemeAt(node.Value.Value) == "5");
}

TEST_CASE("parseFuncCallWithOperator") {
  std::string input = "offset(s1, 5) == 5";
  LlamaParser parser(input, getLexer(input).getTokens());
  FuncNode node;
  REQUIRE_NOTHROW(node = parser.parseFuncCall());
  REQUIRE(node.Value.Name == "offset");
  REQUIRE(node.Value.Args.size() == 2);
  REQUIRE(node.Value.Args.at(0) == "s1");
  REQUIRE(node.Value.Args.at(1) == "5");
  REQUIRE(parser.getLexemeAt(node.Value.Value) == "5");
}

TEST_CASE("parseFactorProducesFuncNodeIfNoParen") {
  std::string input = "any(s1, s2, s3)";
  LlamaParser parser(input, getLexer(input).getTokens());
  std::shared_ptr<Node> node = std::make_shared<FuncNode>();
  REQUIRE_NOTHROW(node = parser.parseFactor(LlamaTokenType::CONDITION));
  REQUIRE(node->Type == NodeType::FUNC);
  auto root = std::static_pointer_cast<FuncNode>(node);
  REQUIRE(root->Value.Name == "any");
  REQUIRE(root->Value.Args.size() == 3);
}

TEST_CASE("parseFactorSignatureSection") {
  std::string input = "name == \"Executable\"";
  LlamaParser parser(input, getLexer(input).getTokens());
  std::shared_ptr<Node> node = std::make_shared<FuncNode>();
  REQUIRE_NOTHROW(node = parser.parseFactor(LlamaTokenType::SIGNATURE));
}

TEST_CASE("parseFactorFileMetadataSection") {
  std::string input = "created == \"2023-04-05\"";
  LlamaParser parser(input, getLexer(input).getTokens());
  std::shared_ptr<Node> node = std::make_shared<FuncNode>();
  REQUIRE_NOTHROW(node = parser.parseFactor(LlamaTokenType::FILE_METADATA));
}

TEST_CASE("parseFactorConditionSection") {
  std::string input = "any(s1, s2, s3)";
  LlamaParser parser(input, getLexer(input).getTokens());
  std::shared_ptr<Node> node = std::make_shared<FuncNode>();
  REQUIRE_NOTHROW(node = parser.parseFactor(LlamaTokenType::CONDITION));
}

TEST_CASE("parseFactorFileMetadataSectionWrongProperty") {
  std::string input = "name == \"Executable\"";
  LlamaParser parser(input, getLexer(input).getTokens());
  std::shared_ptr<Node> node = std::make_shared<FuncNode>();
  REQUIRE_THROWS(node = parser.parseFactor(LlamaTokenType::FILE_METADATA));
}

TEST_CASE("parseFactorSignatureSectionWrongProperty") {
  std::string input = "created > \"2023-04-05\"";
  LlamaParser parser(input, getLexer(input).getTokens());
  std::shared_ptr<Node> node = std::make_shared<FuncNode>();
  REQUIRE_THROWS(node = parser.parseFactor(LlamaTokenType::SIGNATURE));
}

TEST_CASE("parseFactorConditionSectionWrongProperty") {
  std::string input = "created > \"2023-04-05\"";
  LlamaParser parser(input, getLexer(input).getTokens());
  std::shared_ptr<Node> node = std::make_shared<FuncNode>();
  REQUIRE_THROWS(node = parser.parseFactor(LlamaTokenType::CONDITION));
}

TEST_CASE("parseFileHashRecord") {
  std::string input = "md5 == \"test\", sha1 == \"test2\"";
  LlamaParser parser(input, getLexer(input).getTokens());
  FileHashRecord rec;
  REQUIRE_NOTHROW(rec = parser.parseFileHashRecord());
  REQUIRE(findKey(rec, SFHASH_MD5)->second == "\"test\"");
  REQUIRE(findKey(rec, SFHASH_SHA_1)->second == "\"test2\"");
}

TEST_CASE("parseFileHashRecordThrowsIfDuplicateHashType") {
  std::string input = "md5 == \"test\", md5 == \"test2\"";
  LlamaParser parser(input, getLexer(input).getTokens());
  REQUIRE_THROWS_AS(parser.parseFileHashRecord(), ParserError);
}

TEST_CASE("parseConditionFunctionInvalid"){
  SECTION("Invalid function name") {
    std::string input = "invalid()";
    LlamaParser parser(input, getLexer(input).getTokens());
    REQUIRE_THROWS_AS(parser.parseFuncCall(), ParserError);
  }

  SECTION("Invalid function argument") {
    std::string input = "count(arg1, arg2)";
    LlamaParser parser(input, getLexer(input).getTokens());
    REQUIRE_THROWS_AS(parser.parseFuncCall(), ParserError);
  }

  SECTION("Invalid function operator") {
    std::string input = "any(arg1) == 5";
    LlamaParser parser(input, getLexer(input).getTokens());
    REQUIRE_THROWS_AS(parser.parseFuncCall(), ParserError);
  }

  SECTION("Missing operator and/or value") {
    std::string input = "count(arg1)";
    LlamaParser parser(input, getLexer(input).getTokens());
    REQUIRE_THROWS_AS(parser.parseFuncCall(), ParserError);
  }

  SECTION("Missing arguments") {
    std::string input = "offset()";
    LlamaParser parser(input, getLexer(input).getTokens());
    REQUIRE_THROWS_AS(parser.parseFuncCall(), ParserError);
  }
}

TEST_CASE("parseConditionFunctionValid") {
  SECTION("all with zero args") {
    std::string input = "all()";
    LlamaParser parser(input, getLexer(input).getTokens());
    FuncNode func;
    REQUIRE_NOTHROW(func = parser.parseFuncCall());
  }

  SECTION("all with many args") {
    std::string input = "all(arg1, arg2, arg3)";
    LlamaParser parser(input, getLexer(input).getTokens());
    FuncNode func;
    REQUIRE_NOTHROW(func = parser.parseFuncCall());
  }

  SECTION("any with zero args") {
    std::string input = "any()";
    LlamaParser parser(input, getLexer(input).getTokens());
    FuncNode func;
    REQUIRE_NOTHROW(func = parser.parseFuncCall());
  }

  SECTION("any with many args") {
    std::string input = "any(arg1, arg2, arg3)";
    LlamaParser parser(input, getLexer(input).getTokens());
    FuncNode func;
    REQUIRE_NOTHROW(func = parser.parseFuncCall());
  }

  SECTION("count with one arg and comparison") {
    std::string input = "count(arg1) == 5";
    LlamaParser parser(input, getLexer(input).getTokens());
    FuncNode func;
    REQUIRE_NOTHROW(func = parser.parseFuncCall());
  }

  SECTION("length with one arg and comparison") {
    std::string input = "length(arg1) == 5";
    LlamaParser parser(input, getLexer(input).getTokens());
    FuncNode func;
    REQUIRE_NOTHROW(func = parser.parseFuncCall());
  }

  SECTION("length with two args and comparison") {
    std::string input = "length(arg1, 4) == 5";
    LlamaParser parser(input, getLexer(input).getTokens());
    FuncNode func;
    REQUIRE_NOTHROW(func = parser.parseFuncCall());
  }

  SECTION("offset with one arg and comparison") {
    std::string input = "offset(arg1) == 5";
    LlamaParser parser(input, getLexer(input).getTokens());
    FuncNode func;
    REQUIRE_NOTHROW(func = parser.parseFuncCall());
  }

  SECTION("offset with two args and comparison") {
    std::string input = "offset(arg1, 4) == 5";
    LlamaParser parser(input, getLexer(input).getTokens());
    FuncNode func;
    REQUIRE_NOTHROW(func = parser.parseFuncCall());
  }

  SECTION("count_has_hits with zero args and comparison") {
    std::string input = "count_has_hits() > 6";
    LlamaParser parser(input, getLexer(input).getTokens());
    FuncNode func;
    REQUIRE_NOTHROW(func = parser.parseFuncCall());
  }

  SECTION("count_has_hits with many args and comparison") {
    std::string input = "count_has_hits(arg1, arg2, arg3) == 3";
    LlamaParser parser(input, getLexer(input).getTokens());
    FuncNode func;
    REQUIRE_NOTHROW(func = parser.parseFuncCall());
  }
}

TEST_CASE("EmptyStringNoRules") {
  std::string input = "";
  LlamaLexer lexer = getLexer(input);
  LlamaParser parser(input, lexer.getTokens());
  REQUIRE(parser.parseRules(lexer.getRuleCount(), lexer.getRuleIndices()).size() == 0);
}

TEST_CASE("GetSqlQueryFromRule") {
  std::string input = "rule MyRule { }";
  LlamaLexer lexer = getLexer(input);
  LlamaParser parser(input, lexer.getTokens());
  std::vector<Rule> rules = parser.parseRules(lexer.getRuleCount(), lexer.getRuleIndices());
  REQUIRE(rules.at(0).Name == "MyRule");
  REQUIRE(rules.at(0).getSqlQuery(parser) == "SELECT '" + rules.at(0).getHash(parser).to_string() + "', path, name, addr FROM dirent, inode WHERE dirent.metaaddr == inode.addr");
}

TEST_CASE("GetSqlQueryFromRuleWithOneNumberFileMetadataCondition") {
  std::string input = "rule MyRule { file_metadata: filesize == 30000 }";
  LlamaLexer lexer = getLexer(input);
  LlamaParser parser(input, lexer.getTokens());
  std::vector<Rule> rules = parser.parseRules(lexer.getRuleCount(), lexer.getRuleIndices());
  REQUIRE(rules.at(0).Name == "MyRule");
  REQUIRE(rules.at(0).getSqlQuery(parser) == "SELECT '" + rules.at(0).getHash(parser).to_string() + "', path, name, addr FROM dirent, inode WHERE dirent.metaaddr == inode.addr AND filesize == 30000");
}

TEST_CASE("GetSqlQueryFromRuleWithOneStringFileMetadataCondition") {
  std::string input = "rule MyRule { file_metadata: created > \"2023-05-04\" }";
  LlamaLexer lexer = getLexer(input);
  LlamaParser parser(input, lexer.getTokens());
  std::vector<Rule> rules = parser.parseRules(lexer.getRuleCount(), lexer.getRuleIndices());
  REQUIRE(rules.at(0).Name == "MyRule");
  REQUIRE(rules.at(0).getSqlQuery(parser) == "SELECT '" + rules.at(0).getHash(parser).to_string() + "', path, name, addr FROM dirent, inode WHERE dirent.metaaddr == inode.addr AND created > '2023-05-04'");
}

TEST_CASE("GetSqlQueryFromRuleWithCompoundFileMetadataDef") {
  std::string input = "rule MyRule { file_metadata: filesize == 123456 or created > \"2023-05-04\" and modified < \"2023-05-06\" and filename == \"test\" and filepath == \"test\" }";
  LlamaLexer lexer = getLexer(input);
  LlamaParser parser(input, lexer.getTokens());
  std::vector<Rule> rules = parser.parseRules(lexer.getRuleCount(), lexer.getRuleIndices());
  REQUIRE(rules.at(0).Name == "MyRule");
  REQUIRE(rules.at(0).getSqlQuery(parser) == "SELECT '" + rules.at(0).getHash(parser).to_string() + "', path, name, addr FROM dirent, inode WHERE dirent.metaaddr == inode.addr AND (filesize == 123456 OR (((created > '2023-05-04' AND modified < '2023-05-06') AND name == 'test') AND path == 'test'))");
}

TEST_CASE("GetRuleHashWithNoSections") {
  std::string input = "rule MyRule { } rule MyOtherRule { file_metadata: filesize > 30000 }";
  LlamaLexer lexer = getLexer(input);
  LlamaParser parser(input, lexer.getTokens());
  std::vector<Rule> rules = parser.parseRules(lexer.getRuleCount(), lexer.getRuleIndices());
  REQUIRE(rules.at(0).getHash(parser) != rules.at(1).getHash(parser));
}

TEST_CASE("getPreviousLexemeStringInvalidation") {
  std::string input = "rule {}";
  LlamaParser parser(input, getLexer(input).getTokens());
  parser.matchAny(LlamaTokenType::RULE);
  std::string_view sv = parser.getPreviousLexeme();
  REQUIRE(sv == std::string_view("rule"));
}

TEST_CASE("toLlamaOp") {
  REQUIRE(toLlamaOp(LlamaTokenType::EQUAL_EQUAL) == LlamaOp::EQUAL_EQUAL);
  REQUIRE(toLlamaOp(LlamaTokenType::NOT_EQUAL) == LlamaOp::NOT_EQUAL);
  REQUIRE(toLlamaOp(LlamaTokenType::GREATER_THAN) == LlamaOp::GREATER_THAN);
  REQUIRE(toLlamaOp(LlamaTokenType::GREATER_THAN_EQUAL) == LlamaOp::GREATER_THAN_EQUAL);
  REQUIRE(toLlamaOp(LlamaTokenType::LESS_THAN) == LlamaOp::LESS_THAN);
  REQUIRE(toLlamaOp(LlamaTokenType::LESS_THAN_EQUAL) == LlamaOp::LESS_THAN_EQUAL);
}

TEST_CASE("badRuleSkippedOne") {
  std::string input = "rule myBadRule }";
  LlamaLexer lexer = getLexer(input);
  REQUIRE(lexer.getRuleIndices() == std::vector<size_t>{0});
  LlamaParser parser(input, lexer.getTokens());
  std::vector<Rule> rules = parser.parseRules(lexer.getRuleCount(), lexer.getRuleIndices());
  REQUIRE(rules.size() == 0);
}

TEST_CASE("badRuleSkippedTwo") {
  std::string input = "rule myBadRule } rule myGoodRule {}";
  LlamaLexer lexer = getLexer(input);
  REQUIRE(lexer.getRuleIndices() == std::vector<size_t>{0, 3});
  LlamaParser parser(input, lexer.getTokens());
  std::vector<Rule> rules = parser.parseRules(lexer.getRuleCount(), lexer.getRuleIndices());
  REQUIRE(rules.size() == 1);
  REQUIRE(rules[0].Name == "myGoodRule");
}