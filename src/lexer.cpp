#include "lexer.h"

void print(std::string s) {
  std::cout << s << std::endl;
}

void LlamaLexer::scanTokens() {
  while (!isAtEnd()) {
    scanToken();
  }
  addToken(TokenType::END_OF_FILE, CurIdx, CurIdx+1);
}

void LlamaLexer::scanToken() {
  uint32_t start = CurIdx;
  char c = advance();
  switch(c) {
    case '\t':
    case '\n':
    case '\r':
    case ' ': break;

    case '"': parseString(); break;

    case ':': addToken(TokenType::COLON, start, CurIdx); break;
    case '=': addToken(TokenType::EQUAL, start, CurIdx); break;
    case '{': addToken(TokenType::OPEN_BRACE, start, CurIdx); break;
    case '}': addToken(TokenType::CLOSE_BRACE, start, CurIdx); break;

    default:
      if (isdigit(c)) {
        parseNumber();
      }
      else if (isalnum(c)) {
        parseIdentifier();
      }
      else {
        throw UnexpectedInputError("Unexpected input character: " + std::string{c});
      }
  }
}

void LlamaLexer::parseIdentifier() {
  uint32_t start = CurIdx;
  if (CurIdx > 0) {
    start--;
  }
  while (isalnum(getCurChar()) || getCurChar() == '_') {
    advance();
  }

  uint32_t end = CurIdx;
  std::string lexeme = Input.substr(start, end - start);
  auto found = Llama::keywords.find(lexeme);

  if (found != Llama::keywords.end()) {
    addToken(found->second, start, end);
  }
  else {
    addToken(TokenType::IDENTIFIER, start, end);
  }
}

void LlamaLexer::parseString() {
  uint32_t start = CurIdx;
  while(getCurChar() != '"' && !isAtEnd()) {
    advance();
  }
  if (isAtEnd()) {
    throw UnexpectedInputError("Unterminated string");
  }
  uint32_t end = CurIdx;
  advance(); // consume closing quote
  addToken(TokenType::DOUBLE_QUOTED_STRING, start, end);
}

void LlamaLexer::parseNumber() {
  uint32_t start = CurIdx;
  if (CurIdx > 0) {
    start--;
  }
  while (isdigit(getCurChar())) {
    advance();
  }

  uint32_t end = CurIdx;
  std::string lexeme = Input.substr(start, end - start);
  addToken(TokenType::NUMBER, start, end);
}

char LlamaLexer::getCurChar() const {
  if (CurIdx >= Input.size()) {
    return '\0';
  }
  else {
    return Input.at(CurIdx);
  }
}