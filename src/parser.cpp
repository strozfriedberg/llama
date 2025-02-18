#include <iostream>

#include "parser.h"
#include "util.h"

FileHashRecord::const_iterator findKey(const FileHashRecord& container, SFHASH_HashAlgorithm alg) {
  return std::find_if(container.begin(), container.end(), [alg](const auto& x){return x.first == alg;});
}

uint64_t toLlamaOp(LlamaTokenType t) {
    uint64_t res = 0;
    if (t > LlamaTokenType::EQUAL && t < LlamaTokenType::IDENTIFIER) {
      uint64_t shiftAmt = (uint64_t)(t) - (uint64_t)(LlamaTokenType::EQUAL_EQUAL);
      res = 1 << shiftAmt;
    }
    return res;
}

void Function::validate() {
  FunctionProperties props = FunctionValidProperties.find(Name)->second;
  if (props.IsCompFunc && (Operator == SIZE_MAX || Value == SIZE_MAX)) {
    throw ParserError("Expected operator and value for comparison", Pos);
  }
  else if (!props.IsCompFunc && (Operator != SIZE_MAX || Value != SIZE_MAX)) {
    throw ParserError("Unexpected operator or value for function", Pos);
  }

  if (Args.size() < props.MinArgs || Args.size() > props.MaxArgs) {
    throw ParserError("Invalid number of arguments", Pos);
  }
}

void LlamaParser::clear() {
  Tokens.clear();
  Input.clear();
  resetCounters();
}

void LlamaParser::resetCounters() {
  CurIdx     = 0;
  CurRuleIdx = 0;
}

std::string_view LlamaParser::expect(LlamaTokenType token) {
  switch (token) {
    case LlamaTokenType::RULE: mustParse("Expected rule keyword", LlamaTokenType::RULE); break;
    case LlamaTokenType::META: mustParse("Expected meta keyword", LlamaTokenType::META); break;
    case LlamaTokenType::FILE_METADATA: mustParse("Expected file_metadata keyword", LlamaTokenType::FILE_METADATA); break;
    case LlamaTokenType::SIGNATURE: mustParse("Expected signature keyword", LlamaTokenType::SIGNATURE); break;
    case LlamaTokenType::GREP: mustParse("Expected grep keyword", LlamaTokenType::GREP); break;
    case LlamaTokenType::PATTERNS: mustParse("Expected patterns keyword", LlamaTokenType::PATTERNS); break;
    case LlamaTokenType::HASH: mustParse("Expected hash keyword", LlamaTokenType::HASH); break;
    case LlamaTokenType::CONDITION: mustParse("Expected condition keyword", LlamaTokenType::CONDITION); break;
    case LlamaTokenType::OPEN_BRACE: mustParse("Expected open brace", LlamaTokenType::OPEN_BRACE); break;
    case LlamaTokenType::CLOSE_BRACE: mustParse("Expected close brace", LlamaTokenType::CLOSE_BRACE); break;
    case LlamaTokenType::OPEN_PAREN: mustParse("Expected open parenthesis", LlamaTokenType::OPEN_PAREN); break;
    case LlamaTokenType::CLOSE_PAREN: mustParse("Expected close parenthesis", LlamaTokenType::CLOSE_PAREN); break;
    case LlamaTokenType::COLON: mustParse("Expected colon", LlamaTokenType::COLON); break;
    case LlamaTokenType::EQUAL: mustParse("Expected equal sign", LlamaTokenType::EQUAL); break;
    case LlamaTokenType::EQUAL_EQUAL: mustParse("Expected equality operator", LlamaTokenType::EQUAL_EQUAL); break;
    case LlamaTokenType::IDENTIFIER: mustParse("Expected identifier", LlamaTokenType::IDENTIFIER); break;
    case LlamaTokenType::DOUBLE_QUOTED_STRING: mustParse("Expected double quoted string", LlamaTokenType::DOUBLE_QUOTED_STRING); break;
    case LlamaTokenType::NUMBER: mustParse("Expected number", LlamaTokenType::NUMBER); break;
    default:
      throw ParserError("Invalid token type", peek().Pos);
  }
  return previousLexeme();
}

FieldHash Rule::getHash(const LlamaParser& parser) const {
  FieldHasher hasher;
  hasher.hash_iter(parser.Tokens.begin() + Start, parser.Tokens.begin() + End, [](const Token& token) { return token.Lexeme; });
  return hasher.get_hash();
}

HashSection LlamaParser::parseHashSection() {
  HashSection hashSection;
  FileHashRecord rec;
  while (checkAny(LlamaTokenType::MD5, LlamaTokenType::SHA1, LlamaTokenType::SHA256, LlamaTokenType::BLAKE3)) {
    rec = parseFileHashRecord();
    for (const auto& key : rec) {
      hashSection.HashAlgs |= key.first;
    }
    hashSection.FileHashRecords.emplace_back(rec);
  }
  if (!hashSection.HashAlgs) {
    throw ParserError("No hash algorithms specified", peek().Pos);
  }
  return hashSection;
}

SFHASH_HashAlgorithm LlamaParser::parseHash() {
  mustParse(
    "Expected hash type", LlamaTokenType::MD5, LlamaTokenType::SHA1, LlamaTokenType::SHA256, LlamaTokenType::BLAKE3
  );
  switch(previous().Type) {
    case LlamaTokenType::MD5:
      return SFHASH_MD5;
    case LlamaTokenType::SHA1:
      return SFHASH_SHA_1;
    case LlamaTokenType::SHA256:
      return SFHASH_SHA_2_256;
    case LlamaTokenType::BLAKE3:
      return SFHASH_BLAKE3;
    default:
      throw ParserError("Invalid hash type", previous().Pos);
  }
}

FileHashRecord LlamaParser::parseFileHashRecord() {
  FileHashRecord record;
  SFHASH_HashAlgorithm alg = parseHash();
  record.emplace_back(alg, parseHashValue());
  while(matchAny(LlamaTokenType::COMMA)) {
    alg = parseHash();
    if (auto it = findKey(record, alg); it != record.end()) {
      throw ParserError("Duplicate hash type", previous().Pos);
    }
    record.emplace_back(alg, parseHashValue());
  }
  return record;
}

std::string_view LlamaParser::parseHashValue() {
  expect(LlamaTokenType::EQUAL_EQUAL);
  return expect(LlamaTokenType::DOUBLE_QUOTED_STRING);
}

PatternDef LlamaParser::parsePatternMod() {
  LG_KeyOptions opts{0,0,0};
  std::string pat = std::string(previousLexeme());
  Encodings enc{0, 0};

  opts.FixedString = matchAny(LlamaTokenType::FIXED);
  opts.CaseInsensitive = matchAny(LlamaTokenType::NOCASE);
  if (matchAny(LlamaTokenType::ENCODINGS)) {
    enc = parseEncodings();
  }

  return PatternDef{
    LG_KeyOptions{
      opts.FixedString,
      opts.CaseInsensitive,
      /*UnicodeMode=*/true
    },
    enc,
    pat
  };
}

Encodings LlamaParser::parseEncodings() {
  Encodings enc{0,0};
  expect(LlamaTokenType::EQUAL);
  enc.first = CurIdx;

  do {
    expect(LlamaTokenType::IDENTIFIER);
  }
  while (matchAny(LlamaTokenType::COMMA));

  enc.second = CurIdx;
  return enc;
}

PatternDef LlamaParser::parsePatternDef() {
  expect(LlamaTokenType::EQUAL);
  if (matchAny(LlamaTokenType::DOUBLE_QUOTED_STRING)) {
    return parsePatternMod();
  }
  else if (matchAny(LlamaTokenType::OPEN_BRACE)) {
    return parseHexString();
  }
  else {
    throw ParserError("Expected double quoted string or hex string", peek().Pos);
  }
}

PatternSection LlamaParser::parsePatternsSection() {
  PatternSection patternSection;
  while (matchAny(LlamaTokenType::IDENTIFIER)) {
    std::string_view key = previousLexeme();
    patternSection.Patterns.insert(std::make_pair(key, parsePatternDef()));
  }
  if (patternSection.Patterns.empty()) {
    throw ParserError("No patterns specified", peek().Pos);
  }
  return patternSection;
}

PatternDef LlamaParser::parseHexString() {
  std::string hexDigit, hexString;
  while (!checkAny(LlamaTokenType::CLOSE_BRACE) && !isAtEnd()) {
    if (matchAny(LlamaTokenType::IDENTIFIER, LlamaTokenType::NUMBER)) {
      if (isEven(hexString.size())) {
        hexString += "\\z";
      }
      hexDigit = previousLexeme();
      for (char c : hexDigit) {
        if (!isxdigit(c)) {
          throw ParserError("Invalid hex digit", previous().Pos);
        }
      }
      hexString += hexDigit;
    }
    else {
      throw ParserError("Expected hex digit", peek().Pos);
    }
  }
  if (isAtEnd()) {
    throw ParserError("Unterminated hex string", peek().Pos);
  }
  if (isOdd(hexString.size())) {
    throw ParserError("Odd number of hex digits", peek().Pos);
  }
  if (hexString.size() == 0) {
    throw ParserError("Empty hex string", peek().Pos);
  }
  expect(LlamaTokenType::CLOSE_BRACE);
  return {LG_KeyOptions{0,0,0}, Encodings{0,0}, hexString};
}

std::shared_ptr<Node> LlamaParser::parseFactor(LlamaTokenType section) {
  std::shared_ptr<Node> node;
  if (matchAny(LlamaTokenType::OPEN_PAREN)) {
    node = parseExpr(section);
    expect(LlamaTokenType::CLOSE_PAREN);
  }
  else if (section == LlamaTokenType::FILE_METADATA || section == LlamaTokenType::SIGNATURE) {
    node = std::make_shared<PropertyNode>(parseProperty(section));;
  }
  else if (checkFunctionName()) {
    if (section != LlamaTokenType::CONDITION) throw ParserError("Invalid property in section", previous().Pos);
    node = std::make_shared<FuncNode>(parseFuncCall());
  }
  else {
    throw ParserError("Expected function call or signature definition", peek().Pos);
  }
  return node;
}

std::shared_ptr<Node> LlamaParser::parseTerm(LlamaTokenType section) {
  std::shared_ptr<Node> left = parseFactor(section);

  while (matchAny(LlamaTokenType::AND)) {
    std::shared_ptr<BoolNode> node = std::make_shared<BoolNode>();
    node->Operation = BoolNode::Op::AND;
    node->Type = NodeType::BOOL;
    node->Left = left;
    node->Right = parseFactor(section);
    left = node;
  }
  return left;
}

std::shared_ptr<Node> LlamaParser::parseExpr(LlamaTokenType section) {
  std::shared_ptr<Node> left = parseTerm(section);

  while (matchAny(LlamaTokenType::OR)) {
    std::shared_ptr<BoolNode> node = std::make_shared<BoolNode>();
    node->Operation = BoolNode::Op::OR;
    node->Type = NodeType::BOOL;
    node->Left = left;
    node->Right = parseTerm(section);
    left = node;
  }
  return left;
}

FuncNode LlamaParser::parseFuncCall() {
  if (!checkFunctionName()) {
    throw ParserError("Expected function name", peek().Pos);
  }
  advance();
  LineCol pos = peek().Pos;
  std::string_view name = previousLexeme();
  std::vector<std::string_view> args;
  size_t op = SIZE_MAX, val = SIZE_MAX;

  expect(LlamaTokenType::OPEN_PAREN);
  if (matchAny(LlamaTokenType::IDENTIFIER)) {
    args.emplace_back(previousLexeme());
    while (matchAny(LlamaTokenType::COMMA)) {
      mustParse("Expected identifier or number", LlamaTokenType::IDENTIFIER, LlamaTokenType::NUMBER);
      args.emplace_back(previousLexeme());
    }
  }
  expect(LlamaTokenType::CLOSE_PAREN);

  if (matchAny(LlamaTokenType::EQUAL, LlamaTokenType::EQUAL_EQUAL, LlamaTokenType::NOT_EQUAL, LlamaTokenType::GREATER_THAN, LlamaTokenType::GREATER_THAN_EQUAL, LlamaTokenType::LESS_THAN, LlamaTokenType::LESS_THAN_EQUAL)) {
    op = CurIdx - 1;
    expect(LlamaTokenType::NUMBER);
    val = CurIdx - 1;
  }
  Function func(pos, name, std::move(args), op, val);
  return FuncNode(std::move(func));
}

GrepSection LlamaParser::parseGrepSection() {
  GrepSection grepSection;
  expect(LlamaTokenType::PATTERNS);
  expect(LlamaTokenType::COLON);
  grepSection.Patterns = parsePatternsSection();
  expect(LlamaTokenType::CONDITION);
  expect(LlamaTokenType::COLON);
  grepSection.Condition = parseExpr(LlamaTokenType::CONDITION);
  return grepSection;
}

PropertyNode LlamaParser::parseProperty(LlamaTokenType section) {
  Property prop;
  auto sectionSearch = SectionDefs.find(section);
  if (sectionSearch == SectionDefs.end()) {
    throw ParserError("Unexpected section name", peek().Pos);
  }
  const Section& sectionInfo = sectionSearch->second;
  auto propertySearch = sectionInfo.Props.find(currentLexeme());

  if (propertySearch == sectionInfo.Props.end()) {
    throw ParserError("Unexpected property name in section", peek().Pos);
  }
  prop.Name = CurIdx;
  const PropertyInfo& propertyInfo = propertySearch->second;
  advance();

  if ((toLlamaOp(peek().Type) & propertyInfo.ValidOperators) == 0) {
    throw ParserError("Unsupported operator for property", peek().Pos);
  }
  prop.Op = CurIdx;
  advance();

  if (peek().Type != propertyInfo.Type) {
    throw ParserError("Unsupported type for right operand", peek().Pos);
  }
  prop.Val = CurIdx;
  advance();

  return PropertyNode(std::move(prop));
}

MetaSection LlamaParser::parseMetaSection() {
  MetaSection meta;
  while (matchAny(LlamaTokenType::IDENTIFIER)) {
    std::string_view key = previousLexeme();
    expect(LlamaTokenType::EQUAL);
    expect(LlamaTokenType::DOUBLE_QUOTED_STRING);
    std::string_view value = previousLexeme();
    meta.Fields.insert(std::make_pair(key, value));
  }
  return meta;
}

Rule LlamaParser::parseRuleDecl() {
  Rule rule;
  expect(LlamaTokenType::RULE);
  expect(LlamaTokenType::IDENTIFIER);
  rule.Name = previousLexeme();
  expect(LlamaTokenType::OPEN_BRACE);

  if (matchAny(LlamaTokenType::META)) {
    expect(LlamaTokenType::COLON);
    rule.Meta = parseMetaSection();
  }
  rule.Start = CurIdx;
  if (matchAny(LlamaTokenType::HASH)) {
    expect(LlamaTokenType::COLON);
    rule.Hash = parseHashSection();
  }
  if (matchAny(LlamaTokenType::FILE_METADATA)) {
    expect(LlamaTokenType::COLON);
    rule.FileMetadata = parseExpr(LlamaTokenType::FILE_METADATA);
  }
  if (matchAny(LlamaTokenType::SIGNATURE)) {
    expect(LlamaTokenType::COLON);
    rule.Signature = parseExpr(LlamaTokenType::SIGNATURE);
  }
  if (matchAny(LlamaTokenType::GREP)) {
    expect(LlamaTokenType::COLON);
    rule.Grep = parseGrepSection();
  }
  rule.End = CurIdx;
  expect(LlamaTokenType::CLOSE_BRACE);
  return rule;
}

std::vector<Rule> LlamaParser::parseRules(const std::vector<size_t>& ruleIndices) {
  std::vector<Rule> rules;
  rules.reserve(ruleIndices.size());

  // Reset our counters in case parseRules is called twice
  resetCounters();

  while (!isAtEnd() && CurRuleIdx < ruleIndices.size()) {
    if (!checkAny(LlamaTokenType::RULE)) {
      // We should be at the beginning of a rule here
      Errors.emplace_back("Unexpected token " + std::string(peek().Lexeme), peek().Pos);
    }

    // Skip to the next rule index.
    // We do this after the last two checks because we want to report to the user if there are
    // unrecognized characters between known rule boundaries.
    CurIdx = ruleIndices[CurRuleIdx];

    try {
      rules.emplace_back(parseRuleDecl());
    }
    catch (ParserError& e) {
      Errors.push_back(e);
      std::cout << "Rule error: " << e.what() << std::endl;
    }

    // Move to next rule context.
    ++CurRuleIdx;
  }

  if (!isAtEnd()) {
    Errors.emplace_back("Unexpected token " + std::string(peek().Lexeme), peek().Pos);
  }
  return rules;
}

