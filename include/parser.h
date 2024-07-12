#include "token.h"

class ParserError : public std::runtime_error {
public:
  ParserError(const std::string& message) : std::runtime_error(message) {}
};

class LlamaParser {
public:
  LlamaParser(std::vector<Token> tokens) : Tokens(tokens) {}

  Token previous() const { return Tokens.at(CurIdx - 1); }
  Token peek() const { return Tokens.at(CurIdx); }
  Token advance() { if (!isAtEnd()) ++CurIdx; return previous();}

  template <class... TokenTypes>
  bool matchAny(TokenTypes... types);

  template <class... TokenTypes>
  bool checkAny(TokenTypes... types) { return ((peek().Type == types) || ...);};

  bool isAtEnd() const { return peek().Type == TokenType::END_OF_FILE; }

  bool matchHashTokenType();
  bool checkHashTokenType();

  void parseHashSection();
  void parseHashExpr();
  void parseHash();

  std::vector<Token> Tokens;
  uint32_t CurIdx = 0;
};

template <class... TokenTypes>
bool LlamaParser::matchAny(TokenTypes... types) {
  if (checkAny(types...)) {
    advance();
    return true;
  }
  return false;
}

bool LlamaParser::matchHashTokenType() {
  return matchAny(TokenType::MD5, TokenType::SHA1, TokenType::SHA256, TokenType::BLAKE3);
}

bool LlamaParser::checkHashTokenType() {
  return checkAny(TokenType::MD5, TokenType::SHA1, TokenType::SHA256, TokenType::BLAKE3);
}

void LlamaParser::parseHashSection() {
  if (!matchAny(TokenType::HASH)) {
    throw ParserError("Expected hash keyword");
  }
  if (!matchAny(TokenType::COLON)) {
    throw ParserError("Expected colon");
  }
  while (checkHashTokenType()) {
    parseHashExpr();
  }
}

void LlamaParser::parseHashExpr() {
  parseHash();
  if (!matchAny(TokenType::EQUAL)) {
    throw ParserError("Expected equal sign");
  }
  if (!matchAny(TokenType::DOUBLE_QUOTED_STRING)) {
    throw ParserError("Expected double quoted string");
  }
}

void LlamaParser::parseHash() {
  if (!matchHashTokenType()) {
    throw ParserError("Expected hash type");
  }
}