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
  void parseOperator();
  void parseStringMod();
  void parseEncodings();
  void parseStringDef();
  void parseStringsSection();
  void parseNumber();
  void parseDualFuncCall();
  void parseAnyFuncCall();
  void parseAllFuncCall();
  void parseFactor();
  void parseTerm();
  void parseExpr();
  void parseConditionSection();
  void parseSignatureSection();
  void parseGrepSection();
  void parseFileMetadataDef();

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
  mustParse("Expected hash keyword", TokenType::HASH);
  mustParse("Expected colon after hash keyword", TokenType::COLON);
  while (checkAny(TokenType::MD5, TokenType::SHA1, TokenType::SHA256, TokenType::BLAKE3)) {
    parseHashExpr();
  }
}

void LlamaParser::parseHashExpr() {
  parseHash();
  mustParse("Expected equal sign", TokenType::EQUAL);
  mustParse("Expected double quoted string", TokenType::DOUBLE_QUOTED_STRING);
}

void LlamaParser::parseHash() {
  mustParse(
    "Expected hash type", TokenType::MD5, TokenType::SHA1, TokenType::SHA256, TokenType::BLAKE3
  );
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

void LlamaParser::parseStringMod() {
  if (matchAny(TokenType::NOCASE)) {
    return;
  }
  if (matchAny(TokenType::FIXED)) {
    return;
  }
  if (matchAny(TokenType::ENCODINGS)) {
    parseEncodings();
  }
  else{
    throw ParserError("Expected string modifier", peek().Pos);
  }
}

void LlamaParser::parseEncodings() {
  mustParse("Expected equal sign after encodings keyword", TokenType::EQUAL);
  mustParse("Expected encodings list", TokenType::ENCODINGS_LIST);
}

void LlamaParser::parseStringDef() {
  mustParse("Expected identifier", TokenType::IDENTIFIER);
  mustParse("Expected equal sign", TokenType::EQUAL);
  mustParse("Expected double quoted string", TokenType::DOUBLE_QUOTED_STRING);
  while (checkAny(TokenType::NOCASE, TokenType::FIXED, TokenType::ENCODINGS)) {
    parseStringMod();
  }
}

void LlamaParser::parseStringsSection() {
  mustParse("Expected strings keyword", TokenType::STRINGS);
  mustParse("Expected colon after strings keyword", TokenType::COLON);
  while (checkAny(TokenType::IDENTIFIER)) {
    parseStringDef();
  }
}

void LlamaParser::parseNumber() {
  mustParse("Expected number", TokenType::NUMBER);
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

void LlamaParser::parseSignatureSection() {
  mustParse("Expected signature keyword", TokenType::SIGNATURE);
  mustParse("Expected colon after signature keyword", TokenType::COLON);
  mustParse("Expected double quoted string", TokenType::DOUBLE_QUOTED_STRING);
  while (matchAny(TokenType::DOUBLE_QUOTED_STRING)) {}
}

void LlamaParser::parseGrepSection() {
  mustParse("Expected grep keyword", TokenType::GREP);
  mustParse("Expected colon after grep keyword", TokenType::COLON);
  if (checkAny(TokenType::STRINGS)) {
    parseStringsSection();
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