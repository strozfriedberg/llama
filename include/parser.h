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

  template <class... TokenTypes>
  void mustParse(const std::string& errMsg, TokenTypes... types);

  void parseHashSection();
  void parseHashExpr();
  void parseHash();
  void parseArgList();
  void parseOperator();
  void parseConditionFunc();
  void parseFuncCall();

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

template <class... TokenTypes>
void LlamaParser::mustParse(const std::string& errMsg, TokenTypes... types) {
  if (!matchAny(types...)) {
    throw ParserError(errMsg, peek().Pos);
  }
}

void LlamaParser::parseHashSection() {
  mustParse("Expected hash keyword at ", TokenType::HASH);
  mustParse("Expected colon after hash keyword at ", TokenType::COLON);
  while (checkAny(TokenType::MD5, TokenType::SHA1, TokenType::SHA256, TokenType::BLAKE3)) {
    parseHashExpr();
  }
}

void LlamaParser::parseHashExpr() {
  parseHash();
  mustParse("Expected equal sign at ", TokenType::EQUAL);
  mustParse("Expected double quoted string at ", TokenType::DOUBLE_QUOTED_STRING);
}

void LlamaParser::parseHash() {
  mustParse(
    "Expected hash type at ", TokenType::MD5, TokenType::SHA1, TokenType::SHA256, TokenType::BLAKE3
  );
}

void LlamaParser::parseArgList() {
  mustParse("Expected identifier at ", TokenType::IDENTIFIER);
  while (matchAny(TokenType::COMMA)) {
    mustParse("Expected identifier at ", TokenType::IDENTIFIER);
  }
}

void LlamaParser::parseOperator() {
  mustParse(
    "Expected operator at ",
    TokenType::EQUAL_EQUAL,
    TokenType::NOT_EQUAL,
    TokenType::GREATER_THAN,
    TokenType::GREATER_THAN_EQUAL,
    TokenType::LESS_THAN,
    TokenType::LESS_THAN_EQUAL
  );
}

void LlamaParser::parseConditionFunc() {
  mustParse(
    "Expected condition function at ",
    TokenType::ALL,
    TokenType::ANY,
    TokenType::OFFSET,
    TokenType::COUNT,
    TokenType::COUNT_HAS_HITS,
    TokenType::LENGTH
  );
}

void LlamaParser::parseFuncCall() {
  parseConditionFunc();
  mustParse("Expected open parenthesis at ", TokenType::OPEN_PAREN);
  parseArgList();
  mustParse("Expected close parenthesis at ", TokenType::CLOSE_PAREN);
}