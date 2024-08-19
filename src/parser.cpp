#include "parser.h"

void ConditionFunction::assignValidators() {
  switch(Name) {
    case TokenType::ALL:            MinArgs = 0; MaxArgs = 0; IsCompFunc = false;        break;
    case TokenType::ANY:            MinArgs = 1; MaxArgs = SIZE_MAX; IsCompFunc = false; break;
    case TokenType::OFFSET:         MinArgs = 1; MaxArgs = 2; IsCompFunc = true;         break;
    case TokenType::COUNT:          MinArgs = 1; MaxArgs = 1; IsCompFunc = true;         break;
    case TokenType::COUNT_HAS_HITS: MinArgs = 1; MaxArgs = 2; IsCompFunc = true;         break;
    case TokenType::LENGTH:         MinArgs = 1; MaxArgs = 2; IsCompFunc = true;         break;
    default:
      throw ParserError("Invalid function name", Pos);
  }
}

void ConditionFunction::validate() {
  assignValidators();
  if (IsCompFunc) {
    if (Operator == TokenType::NONE || Value.empty()) {
      throw ParserError("Expected operator and value for comparison", Pos);
    }
  }
  else {
    if (Operator != TokenType::NONE || !Value.empty()) {
      throw ParserError("Unexpected operator or value for function", Pos);
    }
  }

  if (Args.size() < MinArgs || Args.size() > MaxArgs) {
    throw ParserError("Invalid number of arguments", Pos);
  }
}

HashSection LlamaParser::parseHashSection() {
  HashSection hashSection;
  FileHashRecord rec;
  while (checkAny(TokenType::MD5, TokenType::SHA1, TokenType::SHA256, TokenType::BLAKE3)) {
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
  while(matchAny(TokenType::COMMA)) {
    alg = parseHash();
    if (record.find(alg) != record.end()) {
      throw ParserError("Duplicate hash type", previous().Pos);
    }
    record[alg] = parseHashValue();
  }
  return record;
}

std::string LlamaParser::parseHashValue() {
  mustParse("Expected equality operator", TokenType::EQUAL_EQUAL);
  mustParse("Expected double quoted string", TokenType::DOUBLE_QUOTED_STRING);
  return getPreviousLexeme();
}

SFHASH_HashAlgorithm LlamaParser::parseHash() {
  mustParse(
    "Expected hash type", TokenType::MD5, TokenType::SHA1, TokenType::SHA256, TokenType::BLAKE3
  );
  switch(previous().Type) {
    case TokenType::MD5:
      return SFHASH_MD5;
    case TokenType::SHA1:
      return SFHASH_SHA_1;
    case TokenType::SHA256:
      return SFHASH_SHA_2_256;
    case TokenType::BLAKE3:
      return SFHASH_BLAKE3;
    default:
      throw ParserError("Invalid hash type", previous().Pos);
  }
}

TokenType LlamaParser::parseOperator() {
  mustParse(
    "Expected operator",
    TokenType::EQUAL_EQUAL,
    TokenType::NOT_EQUAL,
    TokenType::GREATER_THAN,
    TokenType::GREATER_THAN_EQUAL,
    TokenType::LESS_THAN,
    TokenType::LESS_THAN_EQUAL
  );
  return previous().Type;
}

std::vector<PatternDef> LlamaParser::parsePatternMod() {
  const int ASCII = lg_get_encoding_id("ASCII");
  std::vector<PatternDef> defs;
  PatternDef patternDef;
  patternDef.Pattern = getPreviousLexeme();
  std::vector<int> encodings;

  while (checkAny(TokenType::NOCASE, TokenType::FIXED, TokenType::ENCODINGS)) {
    if (matchAny(TokenType::NOCASE)) {
      patternDef.Options.CaseInsensitive = true;
      continue;
    }
    else if (matchAny(TokenType::FIXED)) {
      patternDef.Options.FixedString = true;
      continue;
    }
    else if (matchAny(TokenType::ENCODINGS)) {
      encodings = parseEncodings();
    }
  }

  if (encodings.empty()) {
    encodings.push_back(ASCII);
  }

  for (const int encoding : encodings) {
    PatternDef curDef = patternDef;
    if (encoding != ASCII) {
      curDef.Options.UnicodeMode = true;
    }
    curDef.Encoding = encoding;
    defs.push_back(curDef);
  }

  return defs;
}

std::vector<int> LlamaParser::parseEncodings() {
  std::vector<int> encodings;
  mustParse("Expected equal sign after encodings keyword", TokenType::EQUAL);
  encodings.push_back(parseEncoding());
  while (matchAny(TokenType::COMMA)) {
    encodings.push_back(parseEncoding());
  }
  return encodings;
}

int LlamaParser::parseEncoding () {
  mustParse("Expected encoding", TokenType::IDENTIFIER);
  std::string encoding_lexeme = getPreviousLexeme();
  return lg_get_encoding_id(encoding_lexeme.c_str());
}

std::vector<PatternDef> LlamaParser::parsePatternDef() {
  mustParse("Expected equal sign", TokenType::EQUAL);

  std::vector<PatternDef> defs;
  if (matchAny(TokenType::DOUBLE_QUOTED_STRING)) {
    defs = parsePatternMod();
  }
  else if (matchAny(TokenType::OPEN_BRACE)) {
    defs = parseHexString();
  }
  else {
    throw ParserError("Expected double quoted string or hex string", peek().Pos);
  }
  return defs;
}

PatternSection LlamaParser::parsePatternsSection() {
  PatternSection patternSection;
  while (matchAny(TokenType::IDENTIFIER)) {
    std::string key = getPreviousLexeme();
    patternSection.Patterns.insert(std::make_pair(key, parsePatternDef()));
  }
  if (patternSection.Patterns.empty()) {
    throw ParserError("No patterns specified", peek().Pos);
  }
  return patternSection;
}

std::string LlamaParser::parseNumber() {
  mustParse("Expected number", TokenType::NUMBER);
  return getPreviousLexeme();
}

std::vector<PatternDef> LlamaParser::parseHexString() {
  std::vector<PatternDef> defs;
  PatternDef patternDef;
  std::string hexDigit, hexString;
  while (!checkAny(TokenType::CLOSE_BRACE) && !isAtEnd()) {
    if (matchAny(TokenType::IDENTIFIER, TokenType::NUMBER)) {
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
  mustParse("Expected close brace", TokenType::CLOSE_BRACE);
  patternDef.Pattern = hexString;
  defs.push_back(patternDef);
  return defs;
}

std::shared_ptr<Node> LlamaParser::parseTerm() {
  std::shared_ptr <Node> left = parseFactor();

  while (matchAny(TokenType::AND)) {
    std::shared_ptr<Node> node = std::make_shared<Node>();
    node->Type = NodeType::AND;
    node->Left = left;
    node->Right = parseFactor();
    left = node;
  }
  return left;
}

std::shared_ptr<Node> LlamaParser::parseFactor() {
  std::shared_ptr<Node> node;
  if (matchAny(TokenType::OPEN_PAREN)) {
    node = parseExpr();
    mustParse("Expected close parenthesis", TokenType::CLOSE_PAREN);
  }
  else if (checkAny(TokenType::ANY, TokenType::ALL, TokenType::OFFSET, TokenType::COUNT, TokenType::COUNT_HAS_HITS, TokenType::LENGTH)) {
    auto funcNode = std::make_shared<FuncNode>();
    funcNode->Value = parseFuncCall();
    node = funcNode;
    Atoms.insert(std::make_pair(std::hash<ConditionFunction>{}(funcNode->Value), funcNode->Value));
  }
  else if (checkAny(TokenType::NAME, TokenType::ID)) {
    auto sigDefNode = std::make_shared<SigDefNode>();
    sigDefNode->Value = parseSignatureDef();
    node = sigDefNode;
    Atoms.insert(std::make_pair(std::hash<SignatureDef>{}(sigDefNode->Value), sigDefNode->Value));
  }
  else if (checkAny(TokenType::CREATED, TokenType::MODIFIED, TokenType::FILESIZE, TokenType::FILEPATH, TokenType::FILENAME)) {
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
  mustParse("Expected function name", TokenType::ALL, TokenType::ANY, TokenType::OFFSET, TokenType::COUNT, TokenType::COUNT_HAS_HITS, TokenType::LENGTH);
  func.Name = previous().Type;
  mustParse("Expected open parenthesis", TokenType::OPEN_PAREN);
  if (matchAny(TokenType::IDENTIFIER)) {
    func.Args.push_back(getPreviousLexeme());
  }
  while (matchAny(TokenType::COMMA)) {
    mustParse("Expected identifier or number", TokenType::IDENTIFIER, TokenType::NUMBER);
    func.Args.push_back(getPreviousLexeme());
  }
  mustParse("Expected close parenthesis", TokenType::CLOSE_PAREN);
  if (matchAny(TokenType::EQUAL, TokenType::EQUAL_EQUAL, TokenType::NOT_EQUAL, TokenType::GREATER_THAN, TokenType::GREATER_THAN_EQUAL, TokenType::LESS_THAN, TokenType::LESS_THAN_EQUAL)) {
    func.Operator = previous().Type;
    func.Value = parseNumber();
  }
  func.validate();
  return func;
}

std::shared_ptr<Node> LlamaParser::parseExpr() {
  std::shared_ptr<Node> left = parseTerm();

  while (matchAny(TokenType::OR)) {
    std::shared_ptr<Node> node = std::make_shared<Node>();
    node->Type = NodeType::OR;
    node->Left = left;
    node->Right = parseTerm();
    left = node;
  }
  return left;
}

SignatureDef LlamaParser::parseSignatureDef() {
  SignatureDef def;
  mustParse("Expected name or id keyword", TokenType::NAME, TokenType::ID);
  def.Attr = previous().Type;
  mustParse("Expected equality operator sign", TokenType::EQUAL_EQUAL);
  mustParse("Expected double quoted string", TokenType::DOUBLE_QUOTED_STRING);
  def.Val = getPreviousLexeme();
  return def;
}

GrepSection LlamaParser::parseGrepSection() {
  GrepSection grepSection;
  mustParse("Expected patterns section", TokenType::PATTERNS);
  mustParse("Expected colon", TokenType::COLON);
  grepSection.Patterns = parsePatternsSection();
  mustParse("Expected condition section", TokenType::CONDITION);
  mustParse("Expected colon", TokenType::COLON);
  grepSection.Condition = parseExpr();
  return grepSection;
}

FileMetadataDef LlamaParser::parseFileMetadataDef() {
  FileMetadataDef def;
  if (matchAny(TokenType::CREATED, TokenType::MODIFIED, TokenType::FILEPATH, TokenType::FILENAME)) {
    def.Property = previous().Type;
    def.Operator = parseOperator();
    mustParse("Expected double quoted string", TokenType::DOUBLE_QUOTED_STRING);
    def.Value = getPreviousLexeme();
  }
  else if (matchAny(TokenType::FILESIZE)) {
    def.Property = previous().Type;
    def.Operator = parseOperator();
    def.Value = parseNumber();
  }
  else {
    throw ParserError("Expected created, modified, or filesize", peek().Pos);
  }
  return def;
}

MetaSection LlamaParser::parseMetaSection() {
  MetaSection meta;
  while (matchAny(TokenType::IDENTIFIER)) {
    std::string key = getPreviousLexeme();
    mustParse("Expected equal sign", TokenType::EQUAL);
    mustParse("Expected double quoted string", TokenType::DOUBLE_QUOTED_STRING);
    std::string value = getPreviousLexeme();
    meta.Fields.insert(std::make_pair(key, value));
  }
  return meta;
}

Rule LlamaParser::parseRuleDecl() {
  Rule rule;
  mustParse("Expected rule keyword", TokenType::RULE);
  mustParse("Expected rule name", TokenType::IDENTIFIER);
  rule.Name = getPreviousLexeme();
  mustParse("Expected open curly brace", TokenType::OPEN_BRACE);

  if (matchAny(TokenType::META)) {
    mustParse("Expected colon", TokenType::COLON);
    rule.Meta = parseMetaSection();
  }
  if (matchAny(TokenType::HASH)) {
    mustParse("Expected colon", TokenType::COLON);
    rule.Hash = parseHashSection();
  }
  if (matchAny(TokenType::FILE_METADATA)) {
    mustParse("Expected colon", TokenType::COLON);
    rule.FileMetadata = parseExpr();
  }
  if (matchAny(TokenType::SIGNATURE)) {
    mustParse("Expected colon", TokenType::COLON);
    rule.Signature = parseExpr();
  }
  if (matchAny(TokenType::GREP)) {
    mustParse("Expected colon", TokenType::COLON);
    rule.Grep = parseGrepSection();
  }
  mustParse("Expected close curly brace", TokenType::CLOSE_BRACE);
  return rule;
}

std::vector<Rule> LlamaParser::parseRules() {
  std::vector<Rule> rules;
  while (!isAtEnd()) {
    rules.push_back(parseRuleDecl());
  }
  return rules;
}

