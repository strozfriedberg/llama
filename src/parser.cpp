#include "parser.h"
#include "util.h"

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

std::string BoolNode::getSqlQuery(const LlamaParser& parser) const {
  std::string query = "(";
  query += Left->getSqlQuery(parser);
  query += Type == NodeType::AND ? " AND " : " OR ";
  query += Right->getSqlQuery(parser);
  query += ")";
  return query;
}

std::string FileMetadataNode::getSqlQuery(const LlamaParser& parser) const {
  std::string query = "";
  std::string_view curLex = parser.getLexemeAt(Value.Property);
  query += FileMetadataPropertySqlLookup.find(curLex)->second;
  query += " ";
  query += parser.getLexemeAt(Value.Operator);
  query += " ";
  std::string val = std::string(parser.getLexemeAt(Value.Value));
  std::replace(val.begin(), val.end(), '"', '\'');
  query += val;
  return query;
}

std::string Rule::getSqlQuery(const LlamaParser& parser) const {
  std::string query = "SELECT '";
  query += this->getHash(parser).to_string();
  query += "', path, name, addr FROM dirent, inode WHERE dirent.metaaddr == inode.addr";

  if (FileMetadata) {
    query += " AND ";
    query += FileMetadata->getSqlQuery(parser);
  }

  return query;
}

void LlamaParser::clear() {
  Patterns.clear();
  Tokens.clear();
  Input.clear();
  CurIdx = 0;
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
  return getPreviousLexeme();
}

FieldHash Rule::getHash(const LlamaParser& parser) const {
  FieldHasher hasher;
  hasher.hash_iter(parser.Tokens.begin() + Start, parser.Tokens.begin() + End, [](const Token& token) { return token.Type; });
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
    hashSection.FileHashRecords.push_back(rec);
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
  record[alg] = parseHashValue();
  while(matchAny(LlamaTokenType::COMMA)) {
    alg = parseHash();
    if (record.find(alg) != record.end()) {
      throw ParserError("Duplicate hash type", previous().Pos);
    }
    record[alg] = parseHashValue();
  }
  return record;
}

std::string LlamaParser::parseHashValue() {
  expect(LlamaTokenType::EQUAL_EQUAL);
  return std::string(expect(LlamaTokenType::DOUBLE_QUOTED_STRING));
}

void LlamaParser::parseOperator() {
  mustParse(
    "Expected operator",
    LlamaTokenType::EQUAL_EQUAL,
    LlamaTokenType::NOT_EQUAL,
    LlamaTokenType::GREATER_THAN,
    LlamaTokenType::GREATER_THAN_EQUAL,
    LlamaTokenType::LESS_THAN,
    LlamaTokenType::LESS_THAN_EQUAL
  );
}

std::vector<PatternDef> LlamaParser::parsePatternMod() {
  std::vector<PatternDef> defs;
  PatternDef patternDef;
  patternDef.Pattern = getPreviousLexeme();
  std::vector<std::string> encodings;

  while (matchAny(LlamaTokenType::NOCASE, LlamaTokenType::FIXED, LlamaTokenType::ENCODINGS)) {
    switch(previous().Type) {
      case LlamaTokenType::NOCASE: patternDef.Options.CaseInsensitive = true; break;
      case LlamaTokenType::FIXED: patternDef.Options.FixedString = true; break;
      case LlamaTokenType::ENCODINGS: encodings = parseEncodings(); break;
    }
  }

  if (encodings.empty()) encodings.push_back("ASCII");

  for (const std::string& encoding : encodings) {
    PatternDef curDef = patternDef;
    curDef.Options.UnicodeMode = (encoding != "ASCII");
    curDef.Encoding = encoding;
    defs.push_back(curDef);
  }

  return defs;
}

std::vector<std::string> LlamaParser::parseEncodings() {
  std::vector<std::string> encodings;
  expect(LlamaTokenType::EQUAL);
  encodings.push_back(std::string(expect(LlamaTokenType::IDENTIFIER)));
  while (matchAny(LlamaTokenType::COMMA)) {
    encodings.push_back(std::string(expect(LlamaTokenType::IDENTIFIER)));
  }
  return encodings;
}

std::vector<PatternDef> LlamaParser::parsePatternDef() {
  expect(LlamaTokenType::EQUAL);

  std::vector<PatternDef> defs;
  if (matchAny(LlamaTokenType::DOUBLE_QUOTED_STRING)) {
    defs = parsePatternMod();
  }
  else if (matchAny(LlamaTokenType::OPEN_BRACE)) {
    defs = parseHexString();
  }
  else {
    throw ParserError("Expected double quoted string or hex string", peek().Pos);
  }
  return defs;
}

PatternSection LlamaParser::parsePatternsSection() {
  PatternSection patternSection;
  while (matchAny(LlamaTokenType::IDENTIFIER)) {
    std::string_view key = getPreviousLexeme();
    patternSection.Patterns.insert(std::make_pair(key, parsePatternDef()));
  }
  if (patternSection.Patterns.empty()) {
    throw ParserError("No patterns specified", peek().Pos);
  }
  return patternSection;
}

std::vector<PatternDef> LlamaParser::parseHexString() {
  std::vector<PatternDef> defs;
  PatternDef patternDef;
  std::string hexDigit, hexString;
  while (!checkAny(LlamaTokenType::CLOSE_BRACE) && !isAtEnd()) {
    if (matchAny(LlamaTokenType::IDENTIFIER, LlamaTokenType::NUMBER)) {
      if (isEven(hexString.size())) { // check if hexString is even
        hexString += "\\z";
      }
      hexDigit = getPreviousLexeme();
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
  if (isOdd(hexString.size())) {  // check if hexString is odd
    throw ParserError("Odd number of hex digits", peek().Pos);
  }
  if (hexString.size() == 0) {
    throw ParserError("Empty hex string", peek().Pos);
  }
  expect(LlamaTokenType::CLOSE_BRACE);
  patternDef.Pattern = hexString;
  defs.push_back(patternDef);
  return defs;
}

std::shared_ptr<Node> LlamaParser::parseFactor(LlamaTokenType section) {
  std::shared_ptr<Node> node;
  if (matchAny(LlamaTokenType::OPEN_PAREN)) {
    node = parseExpr(section);
    expect(LlamaTokenType::CLOSE_PAREN);
  }
  else if (checkFunctionName()) {
    if (section != LlamaTokenType::CONDITION) throw ParserError("Invalid property in section", previous().Pos);
    auto funcNode = std::make_shared<FuncNode>(parseFuncCall());
    node = funcNode;
  }
  else if (checkSignatureProperty()) {
    if (section != LlamaTokenType::SIGNATURE) throw ParserError("Invalid property in section", previous().Pos);
    auto sigDefNode = std::make_shared<SigDefNode>(parseSigDef());
    node = sigDefNode;
  }
  else if (checkFileMetadataProperty()) {
    if (section != LlamaTokenType::FILE_METADATA) throw ParserError("Invalid property in section", previous().Pos);
    auto fileMetadataNode = std::make_shared<FileMetadataNode>(parseFileMetadataDef());
    node = fileMetadataNode;
  }
  else {
    throw ParserError("Expected function call or signature definition", peek().Pos);
  }
  return node;
}

std::shared_ptr<Node> LlamaParser::parseTerm(LlamaTokenType section) {
  std::shared_ptr<Node> left = parseFactor(section);

  while (matchAny(LlamaTokenType::AND)) {
    std::shared_ptr<Node> node = std::make_shared<BoolNode>();
    node->Type = NodeType::AND;
    node->Left = left;
    node->Right = parseFactor(section);
    left = node;
  }
  return left;
}

std::shared_ptr<Node> LlamaParser::parseExpr(LlamaTokenType section) {
  std::shared_ptr<Node> left = parseTerm(section);

  while (matchAny(LlamaTokenType::OR)) {
    std::shared_ptr<Node> node = std::make_shared<BoolNode>();
    node->Type = NodeType::OR;
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
  std::string_view name = getPreviousLexeme();
  std::vector<std::string_view> args;
  size_t op = SIZE_MAX, val = SIZE_MAX;
  expect(LlamaTokenType::OPEN_PAREN);
  if (matchAny(LlamaTokenType::IDENTIFIER)) {
    args.push_back(getPreviousLexeme());
  }
  while (matchAny(LlamaTokenType::COMMA)) {
    mustParse("Expected identifier or number", LlamaTokenType::IDENTIFIER, LlamaTokenType::NUMBER);
    args.push_back(getPreviousLexeme());
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

SigDefNode LlamaParser::parseSigDef() {
  SigDef def;
  if (!checkSignatureProperty()) {
    throw ParserError("Expected name or id keyword", peek().Pos);
  }
  advance();
  def.Attr = CurIdx - 1;
  expect(LlamaTokenType::EQUAL_EQUAL);
  expect(LlamaTokenType::DOUBLE_QUOTED_STRING);
  def.Val = CurIdx - 1;
  return SigDefNode(std::move(def));
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

FileMetadataNode LlamaParser::parseFileMetadataDef() {
  FileMetadataDef def;
  bool expectNum = false;

  if (!checkFileMetadataProperty()) {
    throw ParserError("Expected created, modified, filesize, filepath, or filename", peek().Pos);
  }
  advance();

  expectNum = (getPreviousLexeme() == "filesize");
  def.Property = CurIdx - 1;
  parseOperator();
  def.Operator = CurIdx - 1;
  expectNum ? expect(LlamaTokenType::NUMBER) : expect(LlamaTokenType::DOUBLE_QUOTED_STRING);
  def.Value = CurIdx - 1;
  return FileMetadataNode(std::move(def));
}

MetaSection LlamaParser::parseMetaSection() {
  MetaSection meta;
  while (matchAny(LlamaTokenType::IDENTIFIER)) {
    std::string_view key = getPreviousLexeme();
    expect(LlamaTokenType::EQUAL);
    expect(LlamaTokenType::DOUBLE_QUOTED_STRING);
    std::string_view value = getPreviousLexeme();
    meta.Fields.insert(std::make_pair(key, value));
  }
  return meta;
}

Rule LlamaParser::parseRuleDecl() {
  Rule rule;
  expect(LlamaTokenType::RULE);
  expect(LlamaTokenType::IDENTIFIER);
  rule.Name = std::string(getPreviousLexeme());
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

std::vector<Rule> LlamaParser::parseRules() {
  std::vector<Rule> rules;
  while (!isAtEnd()) {
    rules.push_back(parseRuleDecl());
  }
  return rules;
}

