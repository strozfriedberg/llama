#include "parser.h"

void ConditionFunction::assignValidators() {
  switch(Name) {
    case LlamaTokenType::ALL:            MinArgs = 0; MaxArgs = SIZE_MAX; IsCompFunc = false; break;
    case LlamaTokenType::ANY:            MinArgs = 0; MaxArgs = SIZE_MAX; IsCompFunc = false; break;
    case LlamaTokenType::OFFSET:         MinArgs = 1; MaxArgs = 2; IsCompFunc = true;         break;
    case LlamaTokenType::COUNT:          MinArgs = 1; MaxArgs = 1; IsCompFunc = true;         break;
    case LlamaTokenType::COUNT_HAS_HITS: MinArgs = 0; MaxArgs = SIZE_MAX; IsCompFunc = true;  break;
    case LlamaTokenType::LENGTH:         MinArgs = 1; MaxArgs = 2; IsCompFunc = true;         break;
    default:
      throw ParserError("Invalid function name", Pos);
  }
}

void ConditionFunction::validate(const LlamaParser& parser) {
  assignValidators();
  if (IsCompFunc) {
    if (Operator == SIZE_MAX || Value == SIZE_MAX) {
      throw ParserError("Expected operator and value for comparison", Pos);
    }
  }
  else {
    if (Operator != SIZE_MAX || Value != SIZE_MAX) {
      throw ParserError("Unexpected operator or value for function", Pos);
    }
  }

  if (Args.size() < MinArgs || Args.size() > MaxArgs) {
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
  query += parser.getLexemeAt(Value.Property);
  query += " ";
  query += parser.getLexemeAt(Value.Operator);
  query += " ";
  query += parser.getLexemeAt(Value.Value);
  return query;
}

std::string Rule::getSqlQuery(const LlamaParser& parser) const {
  std::string query = "SELECT * FROM inode";

  if (FileMetadata) {
    query += " WHERE ";
    query += FileMetadata->getSqlQuery(parser);
  }

  query += ";";
  return query;
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
  mustParse("Expected equality operator", LlamaTokenType::EQUAL_EQUAL);
  mustParse("Expected double quoted string", LlamaTokenType::DOUBLE_QUOTED_STRING);
  return getPreviousLexeme();
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

  while (checkAny(LlamaTokenType::NOCASE, LlamaTokenType::FIXED, LlamaTokenType::ENCODINGS)) {
    if (matchAny(LlamaTokenType::NOCASE)) {
      patternDef.Options.CaseInsensitive = true;
      continue;
    }
    else if (matchAny(LlamaTokenType::FIXED)) {
      patternDef.Options.FixedString = true;
      continue;
    }
    else if (matchAny(LlamaTokenType::ENCODINGS)) {
      encodings = parseEncodings();
    }
  }

  if (encodings.empty()) {
    encodings.push_back("ASCII");
  }

  for (const std::string& encoding : encodings) {
    PatternDef curDef = patternDef;
    if (encoding != "ASCII") {
      curDef.Options.UnicodeMode = true;
    }
    curDef.Encoding = encoding;
    defs.push_back(curDef);
  }

  return defs;
}

std::vector<std::string> LlamaParser::parseEncodings() {
  std::vector<std::string> encodings;
  mustParse("Expected equal sign after encodings keyword", LlamaTokenType::EQUAL);
  encodings.push_back(parseEncoding());
  while (matchAny(LlamaTokenType::COMMA)) {
    encodings.push_back(parseEncoding());
  }
  return encodings;
}

std::string LlamaParser::parseEncoding () {
  mustParse("Expected encoding", LlamaTokenType::IDENTIFIER);
  return getPreviousLexeme();
}

std::vector<PatternDef> LlamaParser::parsePatternDef() {
  mustParse("Expected equal sign", LlamaTokenType::EQUAL);

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
    std::string key = getPreviousLexeme();
    patternSection.Patterns.insert(std::make_pair(key, parsePatternDef()));
  }
  if (patternSection.Patterns.empty()) {
    throw ParserError("No patterns specified", peek().Pos);
  }
  return patternSection;
}

std::string LlamaParser::parseNumber() {
  mustParse("Expected number", LlamaTokenType::NUMBER);
  return getPreviousLexeme();
}

std::string LlamaParser::parseDoubleQuotedString() {
  mustParse("Expected double-quoted string", LlamaTokenType::DOUBLE_QUOTED_STRING);
  return getPreviousLexeme();
}

std::vector<PatternDef> LlamaParser::parseHexString() {
  std::vector<PatternDef> defs;
  PatternDef patternDef;
  std::string hexDigit, hexString;
  while (!checkAny(LlamaTokenType::CLOSE_BRACE) && !isAtEnd()) {
    if (matchAny(LlamaTokenType::IDENTIFIER, LlamaTokenType::NUMBER)) {
      if (!(hexString.size() & 1)) {
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
  if (hexString.size() & 1) {
    throw ParserError("Odd number of hex digits", peek().Pos);
  }
  if (hexString.size() == 0) {
    throw ParserError("Empty hex string", peek().Pos);
  }
  mustParse("Expected close brace", LlamaTokenType::CLOSE_BRACE);
  patternDef.Pattern = hexString;
  defs.push_back(patternDef);
  return defs;
}

std::shared_ptr<Node> LlamaParser::parseTerm() {
  std::shared_ptr<Node> left = parseFactor();

  while (matchAny(LlamaTokenType::AND)) {
    std::shared_ptr<Node> node = std::make_shared<BoolNode>();
    node->Type = NodeType::AND;
    node->Left = left;
    node->Right = parseFactor();
    left = node;
  }
  return left;
}

std::shared_ptr<Node> LlamaParser::parseFactor() {
  std::shared_ptr<Node> node;
  if (matchAny(LlamaTokenType::OPEN_PAREN)) {
    node = parseExpr();
    mustParse("Expected close parenthesis", LlamaTokenType::CLOSE_PAREN);
  }
  else if (checkAny(LlamaTokenType::ANY, LlamaTokenType::ALL, LlamaTokenType::OFFSET, LlamaTokenType::COUNT, LlamaTokenType::COUNT_HAS_HITS, LlamaTokenType::LENGTH)) {
    auto funcNode = std::make_shared<FuncNode>();
    funcNode->Value = parseFuncCall();
    node = funcNode;
    Atoms.insert(std::make_pair(std::hash<ConditionFunction>{}(funcNode->Value), funcNode->Value));
  }
  else if (checkAny(LlamaTokenType::NAME, LlamaTokenType::ID)) {
    auto sigDefNode = std::make_shared<SigDefNode>();
    sigDefNode->Value = parseSignatureDef();
    node = sigDefNode;
    Atoms.insert(std::make_pair(std::hash<SignatureDef>{}(sigDefNode->Value), sigDefNode->Value));
  }
  else if (checkAny(LlamaTokenType::CREATED, LlamaTokenType::MODIFIED, LlamaTokenType::FILESIZE, LlamaTokenType::FILEPATH, LlamaTokenType::FILENAME)) {
    auto fileMetadataNode = std::make_shared<FileMetadataNode>();
    fileMetadataNode->Value = parseFileMetadataDef();
    node = fileMetadataNode;
    Atoms.insert(std::make_pair(std::hash<FileMetadataDef>{}(fileMetadataNode->Value), fileMetadataNode->Value));
  }
  else {
    throw ParserError("Expected function call or signature definition", peek().Pos);
  }
  return node;
}

ConditionFunction LlamaParser::parseFuncCall() {
  ConditionFunction func(peek().Pos);
  mustParse("Expected function name", LlamaTokenType::ALL, LlamaTokenType::ANY, LlamaTokenType::OFFSET, LlamaTokenType::COUNT, LlamaTokenType::COUNT_HAS_HITS, LlamaTokenType::LENGTH);
  func.Name = previous().Type;
  mustParse("Expected open parenthesis", LlamaTokenType::OPEN_PAREN);
  if (matchAny(LlamaTokenType::IDENTIFIER)) {
    func.Args.push_back(getPreviousLexeme());
  }
  while (matchAny(LlamaTokenType::COMMA)) {
    mustParse("Expected identifier or number", LlamaTokenType::IDENTIFIER, LlamaTokenType::NUMBER);
    func.Args.push_back(getPreviousLexeme());
  }
  mustParse("Expected close parenthesis", LlamaTokenType::CLOSE_PAREN);
  if (matchAny(LlamaTokenType::EQUAL, LlamaTokenType::EQUAL_EQUAL, LlamaTokenType::NOT_EQUAL, LlamaTokenType::GREATER_THAN, LlamaTokenType::GREATER_THAN_EQUAL, LlamaTokenType::LESS_THAN, LlamaTokenType::LESS_THAN_EQUAL)) {
    func.Operator = CurIdx - 1;
    parseNumber();
    func.Value = CurIdx - 1;
  }
  func.validate(*this);
  return func;
}

std::shared_ptr<Node> LlamaParser::parseExpr() {
  std::shared_ptr<Node> left = parseTerm();

  while (matchAny(LlamaTokenType::OR)) {
    std::shared_ptr<Node> node = std::make_shared<BoolNode>();
    node->Type = NodeType::OR;
    node->Left = left;
    node->Right = parseTerm();
    left = node;
  }
  return left;
}

SignatureDef LlamaParser::parseSignatureDef() {
  SignatureDef def;
  mustParse("Expected name or id keyword", LlamaTokenType::NAME, LlamaTokenType::ID);
  def.Attr = CurIdx - 1;
  mustParse("Expected equality operator sign", LlamaTokenType::EQUAL_EQUAL);
  mustParse("Expected double quoted string", LlamaTokenType::DOUBLE_QUOTED_STRING);
  def.Val = CurIdx - 1;
  return def;
}

GrepSection LlamaParser::parseGrepSection() {
  GrepSection grepSection;
  mustParse("Expected patterns section", LlamaTokenType::PATTERNS);
  mustParse("Expected colon", LlamaTokenType::COLON);
  grepSection.Patterns = parsePatternsSection();
  mustParse("Expected condition section", LlamaTokenType::CONDITION);
  mustParse("Expected colon", LlamaTokenType::COLON);
  grepSection.Condition = parseExpr();
  return grepSection;
}

FileMetadataDef LlamaParser::parseFileMetadataDef() {
  FileMetadataDef def;
  bool expectNum = false;

  if (!matchAny(LlamaTokenType::CREATED, LlamaTokenType::MODIFIED, LlamaTokenType::FILESIZE, LlamaTokenType::FILEPATH, LlamaTokenType::FILENAME)) {
    throw ParserError("Expected created, modified, filesize, filepath, or filename", peek().Pos);
  }
  expectNum = (previous().Type == LlamaTokenType::FILESIZE);
  def.Property = CurIdx - 1;
  parseOperator();
  def.Operator = CurIdx - 1;
  expectNum ? parseNumber() : parseDoubleQuotedString();
  def.Value = CurIdx - 1;
  return def;
}

MetaSection LlamaParser::parseMetaSection() {
  MetaSection meta;
  while (matchAny(LlamaTokenType::IDENTIFIER)) {
    std::string key = getPreviousLexeme();
    mustParse("Expected equal sign", LlamaTokenType::EQUAL);
    mustParse("Expected double quoted string", LlamaTokenType::DOUBLE_QUOTED_STRING);
    std::string value = getPreviousLexeme();
    meta.Fields.insert(std::make_pair(key, value));
  }
  return meta;
}

Rule LlamaParser::parseRuleDecl() {
  Rule rule;
  mustParse("Expected rule keyword", LlamaTokenType::RULE);
  mustParse("Expected rule name", LlamaTokenType::IDENTIFIER);
  rule.Name = getPreviousLexeme();
  mustParse("Expected open curly brace", LlamaTokenType::OPEN_BRACE);

  if (matchAny(LlamaTokenType::META)) {
    mustParse("Expected colon", LlamaTokenType::COLON);
    rule.Meta = parseMetaSection();
  }
  if (matchAny(LlamaTokenType::HASH)) {
    mustParse("Expected colon", LlamaTokenType::COLON);
    rule.Hash = parseHashSection();
  }
  if (matchAny(LlamaTokenType::FILE_METADATA)) {
    mustParse("Expected colon", LlamaTokenType::COLON);
    rule.FileMetadata = parseExpr();
  }
  if (matchAny(LlamaTokenType::SIGNATURE)) {
    mustParse("Expected colon", LlamaTokenType::COLON);
    rule.Signature = parseExpr();
  }
  if (matchAny(LlamaTokenType::GREP)) {
    mustParse("Expected colon", LlamaTokenType::COLON);
    rule.Grep = parseGrepSection();
  }
  mustParse("Expected close curly brace", LlamaTokenType::CLOSE_BRACE);
  return rule;
}

std::vector<Rule> LlamaParser::parseRules() {
  std::vector<Rule> rules;
  while (!isAtEnd()) {
    rules.push_back(parseRuleDecl());
  }
  return rules;
}

