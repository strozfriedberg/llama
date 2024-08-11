#include "token.h"

#include <hasher/common.h>
#include <lightgrep/api.h>
#include <lightgrep/util.h>

#include <unordered_map>

class ParserError : public UnexpectedInputError {
public:
  ParserError(const std::string& message, LineCol pos) : UnexpectedInputError(message, pos) {}
};

struct MetaSection {
  std::unordered_map<std::string, std::string> Fields;
};

struct HashExpr {
  SFHASH_HashAlgorithm Alg;
  std::string Val;
};

struct HashSection {
  std::vector<HashExpr> Hashes;
  uint64_t HashAlgs = 0;
};

struct SignatureSection {
  std::vector<std::string> Signatures;
};

struct FileMetadataDef {
  TokenType Property;
  TokenType Operator;
  std::string Value;
};

struct FileMetadataSection {
  std::vector<FileMetadataDef> Fields;
};

struct Rule {
  std::string Name;
  MetaSection Meta;
  HashSection Hash;
  SignatureSection Signature;
};

struct PatternDef {
  std::string Pattern;
  LG_KeyOptions Options = {0,0,0};
  int Encoding;
};

struct PatternSection {
  std::unordered_map<std::string, std::vector<PatternDef>> Patterns;
};

struct ConditionFunction {
  TokenType Name;
  std::vector<std::string> Args;
  TokenType Operator = TokenType::NONE;
  std::string Value;
};

enum class NodeType {
  AND, OR, FUNC, VAL
};

struct Node {
  std::variant<ConditionFunction, std::string> Value;
  NodeType Type;
  std::vector<std::unique_ptr<Node>> Children;
};

class LlamaParser {
public:
  LlamaParser(const std::string& input, const std::vector<Token>& tokens) : Input(input), Tokens(tokens) {}

  Token previous() const { return Tokens.at(CurIdx - 1); }
  Token peek() const { return Tokens.at(CurIdx); }
  Token advance() { if (!isAtEnd()) ++CurIdx; return previous();}

  template <class... TokenTypes>
  bool matchAny(TokenTypes... types);

  template <class... TokenTypes>
  bool checkAny(TokenTypes... types) { return ((peek().Type == types) || ...);};

  bool isAtEnd() const { return peek().Type == TokenType::END_OF_FILE; }

  template <class... TokenTypes>
  void mustParse(const std::string& errMsg, TokenTypes... types);

  HashSection parseHashSection();
  HashExpr parseHashExpr();
  SFHASH_HashAlgorithm parseHash();
  TokenType parseOperator();
  std::vector<PatternDef> parsePatternMod();
  std::vector<int> parseEncodings();
  std::vector<PatternDef> parsePatternDef();
  PatternSection parsePatternsSection();
  std::string parseNumber();
  std::vector<PatternDef> parseHexString();
  ConditionFunction parseFuncCall();
  std::shared_ptr<Node> parseFactor();
  void parseTerm();
  void parseExpr();
  void parseConditionSection();
  SignatureSection parseSignatureSection();
  void parseGrepSection();
  FileMetadataDef parseFileMetadataDef();
  FileMetadataSection parseFileMetadataSection();
  MetaSection parseMetaSection();
  void parseRule(Rule& rule);
  Rule parseRuleDecl();
  std::vector<Rule> parseRules();

  std::string Input;
  std::vector<Token> Tokens;
  std::unordered_map<std::string, std::string> Patterns;
  uint64_t CurIdx = 0;
};

template <class... TokenTypes>
bool LlamaParser::matchAny(TokenTypes... types) {
  if (checkAny(types...)) {
    advance();
    return true;
  }
  return false;
}

template <class... TokenTypes>
void LlamaParser::mustParse(const std::string& errMsg, TokenTypes... types) {
  if (!matchAny(types...)) {
    throw ParserError(errMsg, peek().Pos);
  }
}

HashSection LlamaParser::parseHashSection() {
  mustParse("Expected hash keyword", TokenType::HASH);
  mustParse("Expected colon after hash keyword", TokenType::COLON);
  HashSection hashSection;
  HashExpr hashExpr;
  while (checkAny(TokenType::MD5, TokenType::SHA1, TokenType::SHA256, TokenType::BLAKE3)) {
    hashExpr = parseHashExpr();
    hashSection.HashAlgs |= hashExpr.Alg;
    hashSection.Hashes.push_back(hashExpr);
  }
  return hashSection;
}

HashExpr LlamaParser::parseHashExpr() {
  HashExpr hashExpr;
  hashExpr.Alg = parseHash();
  mustParse("Expected equal sign", TokenType::EQUAL);
  mustParse("Expected double quoted string", TokenType::DOUBLE_QUOTED_STRING);
  hashExpr.Val = Input.substr(previous().Start, previous().length());
  return hashExpr;
}

SFHASH_HashAlgorithm LlamaParser::parseHash() {
  mustParse(
    "Expected hash type", TokenType::MD5, TokenType::SHA1, TokenType::SHA256, TokenType::BLAKE3
  );
  if (previous().Type == TokenType::MD5) {
    return SFHASH_MD5;
  }
  else if (previous().Type == TokenType::SHA1) {
    return SFHASH_SHA_1;
  }
  else if (previous().Type == TokenType::SHA256) {
    return SFHASH_SHA_2_256;
  }
  else {
    return SFHASH_BLAKE3;
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
  patternDef.Pattern = Input.substr(previous().Start, previous().length());
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
  int encoding;
  mustParse("Expected encoding", TokenType::IDENTIFIER);
  encoding = lg_get_encoding_id(Input.substr(previous().Start, previous().length()).c_str());
  encodings.push_back(encoding);
  while (matchAny(TokenType::COMMA)) {
    mustParse("Expected encoding", TokenType::IDENTIFIER);
    encoding = lg_get_encoding_id(Input.substr(previous().Start, previous().length()).c_str());
    encodings.push_back(encoding);
  }
  return encodings;
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
  mustParse("Expected patterns keyword", TokenType::PATTERNS);
  mustParse("Expected colon after patterns keyword", TokenType::COLON);
  PatternSection patternSection;
  while (matchAny(TokenType::IDENTIFIER)) {
    patternSection.Patterns.insert(std::make_pair(
      Input.substr(previous().Start, previous().length()),
      parsePatternDef()
    ));
  }
  return patternSection;
}

std::string LlamaParser::parseNumber() {
  mustParse("Expected number", TokenType::NUMBER);
  return Input.substr(previous().Start, previous().length());
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
      hexDigit = Input.substr(previous().Start, previous().length());
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

void LlamaParser::parseTerm() {
  parseFactor();
  while (matchAny(TokenType::AND)) {
    parseFactor();
  }
}

std::shared_ptr<Node> LlamaParser::parseFactor() {
  auto node = std::make_shared<Node>();
  if (matchAny(TokenType::OPEN_PAREN)) {
    parseExpr();
    mustParse("Expected close parenthesis", TokenType::CLOSE_PAREN);
  }
  else {
    node->Type = NodeType::FUNC;
    node->Value = parseFuncCall();
  }
  return node;
}

ConditionFunction LlamaParser::parseFuncCall() {
  ConditionFunction func;
  mustParse("Expected function name", TokenType::ALL, TokenType::ANY, TokenType::OFFSET, TokenType::COUNT, TokenType::COUNT_HAS_HITS, TokenType::LENGTH);
  func.Name = previous().Type;
  mustParse("Expected open parenthesis", TokenType::OPEN_PAREN);
  if (matchAny(TokenType::IDENTIFIER)) {
    func.Args.push_back(Input.substr(previous().Start, previous().length()));
  }
  while (matchAny(TokenType::COMMA)) {
    mustParse("Expected identifier or number", TokenType::IDENTIFIER, TokenType::NUMBER);
    func.Args.push_back(Input.substr(previous().Start, previous().length()));
  }
  mustParse("Expected close parenthesis", TokenType::CLOSE_PAREN);
  if (matchAny(TokenType::EQUAL, TokenType::EQUAL_EQUAL, TokenType::NOT_EQUAL, TokenType::GREATER_THAN, TokenType::GREATER_THAN_EQUAL, TokenType::LESS_THAN, TokenType::LESS_THAN_EQUAL)) {
    func.Operator = previous().Type;
    func.Value = parseNumber();
  }
  return func;
}

void LlamaParser::parseExpr() {
  parseTerm();
  while (matchAny(TokenType::OR)) {
    parseTerm();
  }
}

void LlamaParser::parseConditionSection() {
  mustParse("Expected condition keyword", TokenType::CONDITION);
  mustParse("Expected colon after condition keyword", TokenType::COLON);
  parseExpr();
}

SignatureSection LlamaParser::parseSignatureSection() {
  SignatureSection signatureSection;
  mustParse("Expected signature keyword", TokenType::SIGNATURE);
  mustParse("Expected colon after signature keyword", TokenType::COLON);
  mustParse("Expected double quoted string", TokenType::DOUBLE_QUOTED_STRING);
  signatureSection.Signatures.push_back(Input.substr(previous().Start, previous().length()));
  while (matchAny(TokenType::DOUBLE_QUOTED_STRING)) {
    signatureSection.Signatures.push_back(Input.substr(previous().Start, previous().length()));
  }
  return signatureSection;
}

void LlamaParser::parseGrepSection() {
  mustParse("Expected grep keyword", TokenType::GREP);
  mustParse("Expected colon after grep keyword", TokenType::COLON);
  if (checkAny(TokenType::PATTERNS)) {
    parsePatternsSection();
  }
  parseConditionSection();
}

FileMetadataDef LlamaParser::parseFileMetadataDef() {
  FileMetadataDef def;
  if (matchAny(TokenType::CREATED, TokenType::MODIFIED, TokenType::FILEPATH, TokenType::FILENAME)) {
    def.Property = previous().Type;
    def.Operator = parseOperator();
    mustParse("Expected double quoted string", TokenType::DOUBLE_QUOTED_STRING);
    def.Value = Input.substr(previous().Start, previous().length());
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

FileMetadataSection LlamaParser::parseFileMetadataSection() {
  mustParse("Expected file_metadata section", TokenType::FILE_METADATA);
  mustParse("Expected colon", TokenType::COLON);

  FileMetadataSection fileMetadataSection;
  while (checkAny(TokenType::CREATED, TokenType::MODIFIED, TokenType::FILESIZE)) {
    fileMetadataSection.Fields.push_back(parseFileMetadataDef());
  }

  return fileMetadataSection;
}

MetaSection LlamaParser::parseMetaSection() {
  MetaSection meta;
  mustParse("Expected meta keyword", TokenType::META);
  mustParse("Expected colon", TokenType::COLON);
  while (matchAny(TokenType::IDENTIFIER)) {
    std::string key = Input.substr(previous().Start, previous().length());
    mustParse("Expected equal sign", TokenType::EQUAL);
    mustParse("Expected double quoted string", TokenType::DOUBLE_QUOTED_STRING);
    std::string value = Input.substr(previous().Start, previous().length());
    meta.Fields.insert(std::make_pair(key, value));
  }
  return meta;
}

void LlamaParser::parseRule(Rule& rule) {
  if (checkAny(TokenType::META)) {
    rule.Meta = parseMetaSection();
  }
  if (checkAny(TokenType::HASH)) {
    rule.Hash = parseHashSection();
  }
  if (checkAny(TokenType::SIGNATURE)) {
    rule.Signature = parseSignatureSection();
  }
  if (checkAny(TokenType::FILE_METADATA)) {
    parseFileMetadataSection();
  }
  if (checkAny(TokenType::GREP)) {
    parseGrepSection();
  }

}

Rule LlamaParser::parseRuleDecl() {
  Rule rule;
  mustParse("Expected rule keyword", TokenType::RULE);
  mustParse("Expected identifier", TokenType::IDENTIFIER);
  rule.Name = Input.substr(previous().Start, previous().length());
  mustParse("Expected open curly brace", TokenType::OPEN_BRACE);
  parseRule(rule);
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

