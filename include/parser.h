#include "token.h"

class ParserError : public UnexpectedInputError {
public:
  ParserError(const std::string& message, LineCol pos) : UnexpectedInputError(message, pos) {}
};

class LlamaParser {
public:
  LlamaParser(const std::vector<Token>& tokens) : Tokens(tokens) {}

  Token previous() const { return Tokens.at(CurIdx - 1); }
  Token peek() const { return Tokens.at(CurIdx); }
  Token advance() { if (!isAtEnd()) ++CurIdx; return previous();}

  template <class... TokenTypes>
  bool matchAny(TokenTypes... types);

  template <class... TokenTypes>
  bool checkAny(TokenTypes... types) { return ((peek().Type == types) || ...);};

  bool isAtEnd() const { return peek().Type == TokenType::END_OF_FILE; }

  void parseHashSection();
  void parseHashExpr();
  void parseHash();

  void parseArgList();

  std::vector<Token> Tokens;
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

void LlamaParser::parseHashSection() {
  if (!matchAny(TokenType::HASH)) {
    throw ParserError("Expected hash keyword at ", peek().Pos);
  }
  if (!matchAny(TokenType::COLON)) {
    throw ParserError("Expected colon at ", peek().Pos);
  }
  while (checkAny(TokenType::MD5, TokenType::SHA1, TokenType::SHA256, TokenType::BLAKE3)) {
    parseHashExpr();
  }
}

void LlamaParser::parseHashExpr() {
  parseHash();
  if (!matchAny(TokenType::EQUAL)) {
    throw ParserError("Expected equal sign at ", peek().Pos);
  }
  if (!matchAny(TokenType::DOUBLE_QUOTED_STRING)) {
    throw ParserError("Expected double quoted string at ", peek().Pos);
  }
}

void LlamaParser::parseHash() {
  if (!matchAny(TokenType::MD5, TokenType::SHA1, TokenType::SHA256, TokenType::BLAKE3)) {
    throw ParserError("Expected hash type at ", peek().Pos);
  }
}

void LlamaParser::parseArgList() {
  if (!matchAny(TokenType::IDENTIFIER)) {
    throw ParserError("Expected argument at ", peek().Pos);
  }
  while (matchAny(TokenType::COMMA)) {
    if (!matchAny(TokenType::IDENTIFIER)) {
      throw ParserError("Expected argument at ", peek().Pos);
    }
  }
}