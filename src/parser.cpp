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

namespace {
// Holds the valid FunctionProperties for each function type. Used in Functions's validate().
static const std::unordered_map<std::string_view, FunctionProperties> FunctionValidProperties {
  {"all",            FunctionProperties{0, SIZE_MAX, false}},
  {"any",            FunctionProperties{0, SIZE_MAX, false}},
  {"offset",         FunctionProperties{1, 2, true}},
  {"count",          FunctionProperties{1, 1, true}},
  {"count_has_hits", FunctionProperties{0, SIZE_MAX, true}},
  {"length",         FunctionProperties{1, 2, true}}
};
}

void Function::validate() {
  const FunctionProperties& props = FunctionValidProperties.find(Name)->second;
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
  Input = std::string_view();
  Errors.clear();
  BoolNodes.clear();
  FuncNodes.clear();
  PropNodes.clear();
  resetCounters();
}

void LlamaParser::resetCounters() {
  CurIdx     = 0;
  CurRuleIdx = 0;
}

std::string_view LlamaParser::expectedErrorMsg(LlamaTokenType token) {
  switch (token) {
    case LlamaTokenType::RULE: return "Expected rule keyword";
    case LlamaTokenType::META: return "Expected meta keyword";
    case LlamaTokenType::FILE_METADATA: return "Expected file_metadata keyword";
    case LlamaTokenType::SIGNATURE: return "Expected signature keyword";
    case LlamaTokenType::GREP: return "Expected grep keyword";
    case LlamaTokenType::PATTERNS: return "Expected patterns keyword";
    case LlamaTokenType::HASH: return "Expected hash keyword";
    case LlamaTokenType::CONDITION: return "Expected condition keyword";
    case LlamaTokenType::OPEN_BRACE: return "Expected open brace";
    case LlamaTokenType::CLOSE_BRACE: return "Expected close brace";
    case LlamaTokenType::OPEN_PAREN: return "Expected open parenthesis";
    case LlamaTokenType::CLOSE_PAREN: return "Expected close parenthesis";
    case LlamaTokenType::COLON: return "Expected colon";
    case LlamaTokenType::EQUAL: return "Expected equal sign";
    case LlamaTokenType::EQUAL_EQUAL: return "Expected equality operator";
    case LlamaTokenType::IDENTIFIER: return "Expected identifier";
    case LlamaTokenType::DOUBLE_QUOTED_STRING: return "Expected double quoted string";
    case LlamaTokenType::NUMBER: return "Expected number";
    default:
      return "Invalid token type";
  }
}

FieldHash Rule::getHash(const LlamaParser& parser, FieldHasher& hasher) const {
  hasher.reset();
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

Node* LlamaParser::parseFactor(LlamaTokenType section) {
  Node* node = nullptr;
  if (matchAny(LlamaTokenType::OPEN_PAREN)) {
    node = parseExpr(section);
    expect(LlamaTokenType::CLOSE_PAREN);
  }
  else if (section == LlamaTokenType::FILE_METADATA || section == LlamaTokenType::SIGNATURE) {
    node = allocPropNode(parseProperty(section));
  }
  else if (checkFunctionName()) {
    if (section != LlamaTokenType::CONDITION) throw ParserError("Invalid property in section", previous().Pos);
    node = allocFuncNode(parseFuncCall());
  }
  else {
    throw ParserError("Expected function call or signature definition", peek().Pos);
  }
  return node;
}

Node* LlamaParser::parseTerm(LlamaTokenType section) {
  Node* left = parseFactor(section);

  while (matchAny(LlamaTokenType::AND)) {
    BoolNode* node = allocBoolNode();
    node->Operation = BoolNode::Op::AND;
    node->Type = NodeType::BOOL;
    node->Left = left;
    node->Right = parseFactor(section);
    left = node;
  }
  return left;
}

Node* LlamaParser::parseExpr(LlamaTokenType section) {
  Node* left = parseTerm(section);

  while (matchAny(LlamaTokenType::OR)) {
    BoolNode* node = allocBoolNode();
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
    }

    // Move to next rule context.
    ++CurRuleIdx;
  }

  if (!isAtEnd()) {
    Errors.emplace_back("Unexpected token " + std::string(peek().Lexeme), peek().Pos);
  }
  return rules;
}

