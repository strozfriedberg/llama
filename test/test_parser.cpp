#include <catch2/catch_test_macros.hpp>

#include "lexer.h"
#include "parser.h"
#include "rulereader.h"

std::vector<Token> getTokensFromString(const std::string& input) {
  LlamaLexer lexer(input);
  lexer.scanTokens();
  return lexer.getTokens();
}

Token makeToken(LlamaTokenType type) {
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
  REQUIRE(parser.previous().Type == LlamaTokenType::RULE);
}

TEST_CASE("TestLlamaParserPeek") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(input, getTokensFromString(input));
  parser.CurIdx = 1;
  REQUIRE(parser.peek().Type == LlamaTokenType::OPEN_BRACE);
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
  REQUIRE(parser.previous().Type == LlamaTokenType::RULE);
  REQUIRE(parser.peek().Type == LlamaTokenType::OPEN_BRACE);
}

TEST_CASE("TestLlamaParserCheckTrue") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE(parser.checkAny(LlamaTokenType::RULE));
}

TEST_CASE("TestLlamaParserCheckFalse") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_FALSE(parser.checkAny(LlamaTokenType::OPEN_BRACE));
}

TEST_CASE("TestLlamaParserMatchTrue") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE(parser.matchAny(LlamaTokenType::RULE));
  REQUIRE(parser.CurIdx == 1);
}

TEST_CASE("TestLlamaParserMatchFalse") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_FALSE(parser.matchAny(LlamaTokenType::OPEN_BRACE));
  REQUIRE(parser.CurIdx == 0);
}

TEST_CASE("TestLlamaParserMatchMultipleTrue") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE(parser.matchAny(LlamaTokenType::RULE, LlamaTokenType::META));
}

TEST_CASE("TestLlamaParserMatchMultipleFalse") {
  std::string input = "rule { meta: description = \"test\" }";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_FALSE(parser.matchAny(LlamaTokenType::OPEN_BRACE, LlamaTokenType::META));
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

TEST_CASE("parseFileHashRecordThrowsIfNotEqualityOperator") {
  std::string input = "md5 \"test\"";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseFileHashRecord(), ParserError);
}

TEST_CASE("parseFileHashRecordThrowsIfNotString") {
  std::string input = "md5 == 1234";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseFileHashRecord(), ParserError);
}

TEST_CASE("parseFileHashRecordDoesNotThrowIfHashAndString") {
  std::string input = "md5 == \"test\"";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseFileHashRecord());
}

TEST_CASE("parseHashSectionThrowsIfNotAHash") {
  std::string input = "notAHash";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseHashSection(), ParserError);
}

TEST_CASE("parseHashSectionDoesNotThrowIfHash") {
  std::string input = "md5 == \"test\"";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseHashSection());
}

TEST_CASE("parseHashSectionMultipleAlg") {
  std::string input = "md5 == \"test\"\nsha1 == \"abcdef\"";
  LlamaParser parser(input, getTokensFromString(input));
  HashSection hashSection;
  REQUIRE_NOTHROW(hashSection = parser.parseHashSection());
  REQUIRE(hashSection.FileHashRecords.at(0).find(SFHASH_MD5)->second == "\"test\"");
  REQUIRE(hashSection.FileHashRecords.at(1).find(SFHASH_SHA_1)->second == "\"abcdef\"");
}

TEST_CASE("parseHashSectionMultipleRecords") {
  std::string input = "md5 == \"test\", sha1 == \"abcdef\"\nmd5 == \"test2\"";
  LlamaParser parser(input, getTokensFromString(input));
  HashSection hashSection;
  REQUIRE_NOTHROW(hashSection = parser.parseHashSection());
  REQUIRE(hashSection.FileHashRecords.size() == 2);
  REQUIRE(hashSection.FileHashRecords.at(0).find(SFHASH_MD5)->second == "\"test\"");
  REQUIRE(hashSection.FileHashRecords.at(0).find(SFHASH_SHA_1)->second == "\"abcdef\"");
  REQUIRE(hashSection.FileHashRecords.at(1).find(SFHASH_MD5)->second == "\"test2\"");
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
  REQUIRE_NOTHROW(parser.parseOperator());
}

TEST_CASE("parseStringModDoesNotThrowIfStringMod") {
  std::string input = "= \"test\" nocase";
  LlamaParser parser(input, getTokensFromString(input));
  std::vector<PatternDef> defs;
  REQUIRE_NOTHROW(defs = parser.parsePatternDef());
  REQUIRE(defs.size() == 1);
  REQUIRE(defs.at(0).Pattern == "\"test\"");
  REQUIRE(defs.at(0).Encoding == "ASCII");
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
  std::vector<std::string> encodings;
  REQUIRE_NOTHROW(encodings = parser.parseEncodings());
  REQUIRE(encodings.size() == 2);
  REQUIRE(encodings.at(0) == "UTF-8");
  REQUIRE(encodings.at(1) == "UTF-16LE");
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

TEST_CASE("parsePatternsSectionThrowsIfNotIdentifier") {
  std::string input = "rule";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parsePatternsSection(), ParserError);
}

TEST_CASE("parsePatternsSectionDoesNotThrowIfPatterns") {
  std::string input = R"(
  a = "test" encodings=UTF-8 nocase fixed
  b = "test2" encodings=UTF-8 nocase fixed
  c = { 12 34 56 78 9a bc de f0 }
  )";
  LlamaParser parser(input, getTokensFromString(input));
  PatternSection patternSection;
  REQUIRE_NOTHROW(patternSection = parser.parsePatternsSection());
  REQUIRE(patternSection.Patterns.size() == 3);
  REQUIRE(patternSection.Patterns.find("a")->second.at(0).Pattern == "\"test\"");
  REQUIRE(patternSection.Patterns.find("b")->second.at(0).Pattern == "\"test2\"");
  REQUIRE(patternSection.Patterns.find("c")->second.at(0).Pattern == "\\z12\\z34\\z56\\z78\\z9a\\zbc\\zde\\zf0");
}

TEST_CASE("parseTermWithAnd") {
  std::string input = "any(s1, s2, s3) and count(s1) == 5";
  LlamaParser parser(input, getTokensFromString(input));
  std::shared_ptr<Node> node = std::make_shared<BoolNode>();
  REQUIRE_NOTHROW(node = parser.parseTerm());
  REQUIRE(node->Type == NodeType::AND);
  auto left = std::static_pointer_cast<FuncNode>(node->Left);
  REQUIRE(node->Left->Type == NodeType::FUNC);
  REQUIRE(left->Value.Name == LlamaTokenType::ANY);
  auto right = std::static_pointer_cast<FuncNode>(node->Right);
  REQUIRE(node->Right->Type == NodeType::FUNC);
  REQUIRE(right->Value.Name == LlamaTokenType::COUNT);
}

TEST_CASE("parseTermWithoutAnd") {
  std::string input = "any()";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseTerm());
}

TEST_CASE("parseExpr") {
  std::string input = "(any(s1, s2, s3) and length(s1, 5) == 5) or all()";
  LlamaParser parser(input, getTokensFromString(input));
  std::shared_ptr<Node> node = std::make_shared<BoolNode>();
  REQUIRE_NOTHROW(node = parser.parseExpr());
  REQUIRE(node->Type == NodeType::OR);
  REQUIRE(node->Left);
  REQUIRE(node->Left->Type == NodeType::AND);
  REQUIRE(node->Right);
  REQUIRE(node->Right->Type == NodeType::FUNC);
}

TEST_CASE("parseConditionSection") {
  std::string input = "(any(s1, s2, s3) and count(s1) == 5) or all(s1, s2, s3)";
  LlamaParser parser(input, getTokensFromString(input));
  std::shared_ptr<Node> node;
  REQUIRE_NOTHROW(node = parser.parseExpr());
  REQUIRE(parser.CurIdx == parser.Tokens.size() - 1);
  REQUIRE(node->Type == NodeType::OR);
}

TEST_CASE("parseSignatureSection") {
  std::string input = "name == \"Executable\" or id == \"123456789\"";
  LlamaParser parser(input, getTokensFromString(input));
  std::shared_ptr<Node> node;
  REQUIRE_NOTHROW(node = parser.parseExpr());
  REQUIRE(node->Type == NodeType::OR);
  auto sigDefNodeLeft = std::static_pointer_cast<SigDefNode>(node->Left);
  REQUIRE(node->Left->Type == NodeType::SIG);
  REQUIRE(parser.getLexemeAt(sigDefNodeLeft->Value.Attr) == "name");
  REQUIRE(parser.getLexemeAt(sigDefNodeLeft->Value.Val) == "\"Executable\"");
  auto sigDefNodeRight = std::static_pointer_cast<SigDefNode>(node->Right);
  REQUIRE(node->Right->Type == NodeType::SIG);
  REQUIRE(parser.getLexemeAt(sigDefNodeRight->Value.Attr) == "id");
  REQUIRE(parser.getLexemeAt(sigDefNodeRight->Value.Val) == "\"123456789\"");
}

TEST_CASE("parseGrepSection") {
  std::string input = R"(
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
  REQUIRE(section.Patterns.Patterns.find("a")->second.at(0).Pattern == "\"test\"");
  REQUIRE(section.Patterns.Patterns.find("a")->second.at(0).Encoding == "UTF-8");
  REQUIRE(section.Condition->Type == NodeType::AND);
  REQUIRE(section.Condition->Left->Type == NodeType::FUNC);
  REQUIRE(section.Condition->Right->Type == NodeType::FUNC);
}

TEST_CASE("parseFileMetadataDefFileSize") {
  std::string input = "filesize > 100";
  LlamaParser parser(input, getTokensFromString(input));
  FileMetadataDef def;
  REQUIRE_NOTHROW(def = parser.parseFileMetadataDef());
  REQUIRE(parser.getLexemeAt(def.Property) == "filesize");
  REQUIRE(parser.getLexemeAt(def.Operator) == ">");
  REQUIRE(parser.getLexemeAt(def.Value) == "100");
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
  std::string input = R"(created > "2023-05-04" or (modified < "2023-05-06" and filesize >= 100))";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_NOTHROW(parser.parseExpr());
}

TEST_CASE("parseMetaSection") {
  std::string input = R"(
    arbitrary = "something"
    another = "something else"
  )";
  LlamaParser parser(input, getTokensFromString(input));
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
  LlamaParser parser(input, getTokensFromString(input));
  Rule rule;
  REQUIRE_NOTHROW(rule = parser.parseRuleDecl());
  REQUIRE(rule.Hash.FileHashRecords.size() == 1);
  auto root = std::static_pointer_cast<SigDefNode>(rule.Signature);
  REQUIRE(parser.getLexemeAt(root->Value.Attr) == "name");
  REQUIRE(parser.getLexemeAt(root->Value.Val) == "\"Executable\"");
  REQUIRE(parser.Atoms.size() == 3);
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
    hash: md5 == "test"
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
      name == "Executable"
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
  REQUIRE(rules.at(0).Meta.Fields.find("description")->second == "\"test\"");
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
  REQUIRE_NOTHROW(parser.parseNumber());
}

TEST_CASE("parseFuncCallAny") {
  std::string input = "any(s1, s2, s3)";
  LlamaParser parser(input, getTokensFromString(input));
  ConditionFunction func;
  REQUIRE_NOTHROW(func = parser.parseFuncCall());
  REQUIRE(func.Name == LlamaTokenType::ANY);
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
  REQUIRE(func.Name == LlamaTokenType::ALL);
  REQUIRE(func.Args.size() == 0);
}

TEST_CASE("parseFuncCallWithNumber") {
  std::string input = "count(s1) == 5";
  LlamaParser parser(input, getTokensFromString(input));
  ConditionFunction func;
  REQUIRE_NOTHROW(func = parser.parseFuncCall());
  REQUIRE(func.Name == LlamaTokenType::COUNT);
  REQUIRE(func.Args.size() == 1);
  REQUIRE(func.Args.at(0) == "s1");
  REQUIRE(parser.getLexemeAt(func.Operator) == "==");
  REQUIRE(parser.getLexemeAt(func.Value) == "5");
}

TEST_CASE("parseFuncCallWithOperator") {
  std::string input = "offset(s1, 5) == 5";
  LlamaParser parser(input, getTokensFromString(input));
  ConditionFunction func;
  REQUIRE_NOTHROW(func = parser.parseFuncCall());
  REQUIRE(func.Name == LlamaTokenType::OFFSET);
  REQUIRE(func.Args.size() == 2);
  REQUIRE(func.Args.at(0) == "s1");
  REQUIRE(func.Args.at(1) == "5");
  REQUIRE(parser.getLexemeAt(func.Value) == "5");
}

TEST_CASE("parseFactorProducesFuncNodeIfNoParen") {
  std::string input = "any(s1, s2, s3)";
  LlamaParser parser(input, getTokensFromString(input));
  std::shared_ptr<Node> node = std::make_shared<FuncNode>();
  REQUIRE_NOTHROW(node = parser.parseFactor());
  REQUIRE(node->Type == NodeType::FUNC);
  auto root = std::static_pointer_cast<FuncNode>(node);
  REQUIRE(root->Value.Name == LlamaTokenType::ANY);
  REQUIRE(root->Value.Args.size() == 3);
}

TEST_CASE("parseFileHashRecord") {
  std::string input = "md5 == \"test\", sha1 == \"test2\"";
  LlamaParser parser(input, getTokensFromString(input));
  FileHashRecord rec;
  REQUIRE_NOTHROW(rec = parser.parseFileHashRecord());
  REQUIRE(rec.find(SFHASH_MD5)->second == "\"test\"");
  REQUIRE(rec.find(SFHASH_SHA_1)->second == "\"test2\"");
}

TEST_CASE("parseFileHashRecordThrowsIfDuplicateHashType") {
  std::string input = "md5 == \"test\", md5 == \"test2\"";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE_THROWS_AS(parser.parseFileHashRecord(), ParserError);
}

TEST_CASE("parseConditionFunctionInvalid"){
  SECTION("Invalid function name") {
    std::string input = "invalid()";
    LlamaParser parser(input, getTokensFromString(input));
    REQUIRE_THROWS_AS(parser.parseFuncCall(), ParserError);
  }

  SECTION("Invalid function argument") {
    std::string input = "count(arg1, arg2)";
    LlamaParser parser(input, getTokensFromString(input));
    REQUIRE_THROWS_AS(parser.parseFuncCall(), ParserError);
  }

  SECTION("Invalid function operator") {
    std::string input = "any(arg1) == 5";
    LlamaParser parser(input, getTokensFromString(input));
    REQUIRE_THROWS_AS(parser.parseFuncCall(), ParserError);
  }

  SECTION("Missing operator and/or value") {
    std::string input = "count(arg1)";
    LlamaParser parser(input, getTokensFromString(input));
    REQUIRE_THROWS_AS(parser.parseFuncCall(), ParserError);
  }

  SECTION("Missing arguments") {
    std::string input = "offset()";
    LlamaParser parser(input, getTokensFromString(input));
    REQUIRE_THROWS_AS(parser.parseFuncCall(), ParserError);
  }
}

TEST_CASE("parseConditionFunctionValid") {
  SECTION("all with zero args") {
    std::string input = "all()";
    LlamaParser parser(input, getTokensFromString(input));
    ConditionFunction func;
    REQUIRE_NOTHROW(func = parser.parseFuncCall());
  }

  SECTION("all with many args") {
    std::string input = "all(arg1, arg2, arg3)";
    LlamaParser parser(input, getTokensFromString(input));
    ConditionFunction func;
    REQUIRE_NOTHROW(func = parser.parseFuncCall());
  }

  SECTION("any with zero args") {
    std::string input = "any()";
    LlamaParser parser(input, getTokensFromString(input));
    ConditionFunction func;
    REQUIRE_NOTHROW(func = parser.parseFuncCall());
  }

  SECTION("any with many args") {
    std::string input = "any(arg1, arg2, arg3)";
    LlamaParser parser(input, getTokensFromString(input));
    ConditionFunction func;
    REQUIRE_NOTHROW(func = parser.parseFuncCall());
  }

  SECTION("count with one arg and comparison") {
    std::string input = "count(arg1) == 5";
    LlamaParser parser(input, getTokensFromString(input));
    ConditionFunction func;
    REQUIRE_NOTHROW(func = parser.parseFuncCall());
  }

  SECTION("length with one arg and comparison") {
    std::string input = "length(arg1) == 5";
    LlamaParser parser(input, getTokensFromString(input));
    ConditionFunction func;
    REQUIRE_NOTHROW(func = parser.parseFuncCall());
  }

  SECTION("length with two args and comparison") {
    std::string input = "length(arg1, 4) == 5";
    LlamaParser parser(input, getTokensFromString(input));
    ConditionFunction func;
    REQUIRE_NOTHROW(func = parser.parseFuncCall());
  }

  SECTION("offset with one arg and comparison") {
    std::string input = "offset(arg1) == 5";
    LlamaParser parser(input, getTokensFromString(input));
    ConditionFunction func;
    REQUIRE_NOTHROW(func = parser.parseFuncCall());
  }

  SECTION("offset with two args and comparison") {
    std::string input = "offset(arg1, 4) == 5";
    LlamaParser parser(input, getTokensFromString(input));
    ConditionFunction func;
    REQUIRE_NOTHROW(func = parser.parseFuncCall());
  }

  SECTION("count_has_hits with zero args and comparison") {
    std::string input = "count_has_hits() > 6";
    LlamaParser parser(input, getTokensFromString(input));
    ConditionFunction func;
    REQUIRE_NOTHROW(func = parser.parseFuncCall());
  }

  SECTION("count_has_hits with many args and comparison") {
    std::string input = "count_has_hits(arg1, arg2, arg3) == 3";
    LlamaParser parser(input, getTokensFromString(input));
    ConditionFunction func;
    REQUIRE_NOTHROW(func = parser.parseFuncCall());
  }
}

TEST_CASE("EmptyStringNoRules") {
  std::string input = "";
  LlamaParser parser(input, getTokensFromString(input));
  REQUIRE(parser.parseRules().size() == 0);
}

TEST_CASE("GetSqlQueryFromRule") {
  std::string input = "rule MyRule { }";
  LlamaParser parser(input, getTokensFromString(input));
  std::vector<Rule> rules = parser.parseRules();
  REQUIRE(rules.at(0).Name == "MyRule");
  REQUIRE(rules.at(0).getSqlQuery(parser) == "SELECT * FROM inode;");
}

TEST_CASE("GetSqlQueryFromRuleWithOneNumberFileMetadataCondition") {
  std::string input = "rule MyRule { file_metadata: filesize == 30000 }";
  LlamaParser parser(input, getTokensFromString(input));
  std::vector<Rule> rules = parser.parseRules();
  REQUIRE(rules.at(0).Name == "MyRule");
  REQUIRE(rules.at(0).getSqlQuery(parser) == "SELECT * FROM inode WHERE filesize == 30000;");
}

TEST_CASE("GetSqlQueryFromRuleWithOneStringFileMetadataCondition") {
  std::string input = "rule MyRule { file_metadata: created > \"2023-05-04\" }";
  LlamaParser parser(input, getTokensFromString(input));
  std::vector<Rule> rules = parser.parseRules();
  REQUIRE(rules.at(0).Name == "MyRule");
  REQUIRE(rules.at(0).getSqlQuery(parser) == "SELECT * FROM inode WHERE created > \"2023-05-04\";");
}

TEST_CASE("GetSqlQueryFromRuleWithCompoundFileMetadataDef") {
  std::string input = "rule MyRule { file_metadata: filesize == 123456 or created > \"2023-05-04\" and modified < \"2023-05-06\" }";
  LlamaParser parser(input, getTokensFromString(input));
  std::vector<Rule> rules = parser.parseRules();
  REQUIRE(rules.at(0).Name == "MyRule");
  REQUIRE(rules.at(0).getSqlQuery(parser) == "SELECT * FROM inode WHERE (filesize == 123456 OR (created > \"2023-05-04\" AND modified < \"2023-05-06\"));");
}
