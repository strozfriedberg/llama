#include "token.h"

#include <hasher/common.h>

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
};

struct SignatureSection {
  std::vector<std::string> Signatures;
};

struct FileMetadataExpr {
  TokenType Property;
  TokenType Operator;
  std::string Value;
};

struct Rule {
  std::string Name;
  MetaSection Meta;
  HashSection Hash;
  SignatureSection Signature;
};

struct PatternMod {
  bool NoCase = false;
  bool Fixed = false;
  std::vector<std::string> Encodings;
};

struct PatternDef {
  std::string Pattern;
  PatternMod Mod;
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
  void parseOperator();
  void parsePatternMod();
  std::vector<std::string> parseEncodings();
  void parsePatternDef();
  void parsePatternsSection();
  void parseNumber();
  std::string parseHexString();
  void parseDualFuncCall();
  void parseAnyFuncCall();
  void parseAllFuncCall();
  void parseFactor();
  void parseTerm();
  void parseExpr();
  void parseConditionSection();
  SignatureSection parseSignatureSection();
  void parseGrepSection();
  void parseFileMetadataDef();
  void parseFileMetadataSection();
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
  while (checkAny(TokenType::MD5, TokenType::SHA1, TokenType::SHA256, TokenType::BLAKE3)) {
    hashSection.Hashes.push_back(parseHashExpr());
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

void LlamaParser::parseOperator() {
  mustParse(
    "Expected operator",
    TokenType::EQUAL_EQUAL,
    TokenType::NOT_EQUAL,
    TokenType::GREATER_THAN,
    TokenType::GREATER_THAN_EQUAL,
    TokenType::LESS_THAN,
    TokenType::LESS_THAN_EQUAL
  );
}

void LlamaParser::parsePatternMod() {
  PatternMod mod;
  while (checkAny(TokenType::NOCASE, TokenType::FIXED, TokenType::ENCODINGS)) {
    if (matchAny(TokenType::NOCASE)) {
      mod.NoCase = true;
      continue;
    }
    else if (matchAny(TokenType::FIXED)) {
      mod.Fixed = true;
      continue;
    }
    else if (matchAny(TokenType::ENCODINGS)) {
      mod.Encodings = parseEncodings();
    }
  }
}

std::vector<std::string> LlamaParser::parseEncodings() {
  std::vector<std::string> encodings;
  mustParse("Expected equal sign after encodings keyword", TokenType::EQUAL);
  std::string encoding;
  mustParse("Expected encoding", TokenType::IDENTIFIER);
  encodings.push_back(Input.substr(previous().Start, previous().length()));
  while (matchAny(TokenType::COMMA)) {
    mustParse("Expected encoding", TokenType::IDENTIFIER);
    encodings.push_back(Input.substr(previous().Start, previous().length()));
  }
  return encodings;
}

void LlamaParser::parsePatternDef() {
  mustParse("Expected identifier", TokenType::IDENTIFIER);
  mustParse("Expected equal sign", TokenType::EQUAL);

  if (matchAny(TokenType::DOUBLE_QUOTED_STRING)) {
    parsePatternMod();
  }
  else if (matchAny(TokenType::OPEN_BRACE)) {
    parseHexString();
  }
  else {
    throw ParserError("Expected double quoted string or hex string", peek().Pos);
  }
}

void LlamaParser::parsePatternsSection() {
  mustParse("Expected patterns keyword", TokenType::PATTERNS);
  mustParse("Expected colon after patterns keyword", TokenType::COLON);
  while (checkAny(TokenType::IDENTIFIER)) {
    parsePatternDef();
  }
}

void LlamaParser::parseNumber() {
  mustParse("Expected number", TokenType::NUMBER);
}

std::string LlamaParser::parseHexString() {
  std::string hexDigit, hexString;
  while (!checkAny(TokenType::CLOSE_BRACE) && !isAtEnd()) {
    if (matchAny(TokenType::IDENTIFIER, TokenType::NUMBER)) {
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
  return hexString;
}

void LlamaParser::parseDualFuncCall() {
  mustParse(
    "Expected function name",
    TokenType::OFFSET,
    TokenType::COUNT,
    TokenType::COUNT_HAS_HITS,
    TokenType::LENGTH
  );
  mustParse("Expected open parenthesis", TokenType::OPEN_PAREN);
  mustParse("Expected identifier", TokenType::IDENTIFIER);
  if (matchAny(TokenType::COMMA)) {
    parseNumber();
  }
  mustParse("Expected close parenthesis", TokenType::CLOSE_PAREN);
  parseOperator();
  parseNumber();
}

void LlamaParser::parseAnyFuncCall() {
  mustParse("Expected function name", TokenType::ANY);
  mustParse("Expected open parenthesis", TokenType::OPEN_PAREN);
  mustParse("Expected identifier", TokenType::IDENTIFIER);
  while (matchAny(TokenType::COMMA)) {
    mustParse("Expected identifier", TokenType::IDENTIFIER);
  }
  mustParse("Expected close parenthesis", TokenType::CLOSE_PAREN);
}

void LlamaParser::parseAllFuncCall() {
  mustParse("Expected function name", TokenType::ALL);
  mustParse("Expected open parenthesis", TokenType::OPEN_PAREN);
  mustParse("Expected close parenthesis", TokenType::CLOSE_PAREN);
}

void LlamaParser::parseTerm() {
  parseFactor();
  while (matchAny(TokenType::AND)) {
    parseFactor();
  }
}

void LlamaParser::parseFactor() {
  if (matchAny(TokenType::OPEN_PAREN)) {
    parseExpr();
  }
  else if (checkAny(TokenType::ALL)) {
    parseAllFuncCall();
  }
  else if (checkAny(TokenType::ANY)) {
    parseAnyFuncCall();
  }
  else if (checkAny(
    TokenType::OFFSET,
    TokenType::COUNT,
    TokenType::COUNT_HAS_HITS,
    TokenType::LENGTH
  )) {
    parseDualFuncCall();
  }
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

void LlamaParser::parseFileMetadataDef() {
  if (matchAny(TokenType::CREATED, TokenType::MODIFIED)) {
    parseOperator();
    mustParse("Expected double quoted string containing date", TokenType::DOUBLE_QUOTED_STRING);
  }
  else if (matchAny(TokenType::FILESIZE)) {
    parseOperator();
    mustParse("Expected number", TokenType::NUMBER);
  }
  else {
    throw ParserError("Expected created, modified, or filesize", peek().Pos);
  }
}

void LlamaParser::parseFileMetadataSection() {
  mustParse("Expected file_metadata section", TokenType::FILE_METADATA);
  mustParse("Expected colon", TokenType::COLON);
  while (checkAny(TokenType::CREATED, TokenType::MODIFIED, TokenType::FILESIZE)) {
    parseFileMetadataDef();
  }
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

