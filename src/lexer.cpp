#include "lexer.h"

void print(std::string s) {
  std::cout << s << std::endl;
}

void LlamaLexer::scanTokens() {
  while (!isAtEnd()) {
    scanToken();
  }
  addToken(TokenType::END_OF_FILE, CurIdx, CurIdx+1, LineNum, ColNum);
}

void LlamaLexer::scanToken() {
  uint32_t start = CurIdx, curLine = LineNum, curCol = ColNum;
  char c = advance();
  switch(c) {
    case '\t': break;
    case '\n': ColNum = 1; LineNum++; break;
    case '\r':
    case ' ': break;

    case '!' : {
      if (match('=')) {
        addToken(TokenType::NOT_EQUAL, start, CurIdx, curLine, curCol);
      }
      else {
        throw UnexpectedInputError("Unexpected input character: !");
      }
      break;
    }

    case '"': parseString(); break;

    case '(': addToken(TokenType::OPEN_PAREN, start, CurIdx, curLine, curCol); break;
    case ')': addToken(TokenType::CLOSE_PAREN, start, CurIdx, curLine, curCol); break;
    case ',': addToken(TokenType::COMMA, start, CurIdx, curLine, curCol); break;
    case ':': addToken(TokenType::COLON, start, CurIdx, curLine, curCol); break;
    case '<': addToken(match('=') ? TokenType::LESS_THAN_EQUAL : TokenType::LESS_THAN, start, CurIdx, curLine, curCol); break;
    case '=': addToken(match('=') ? TokenType::EQUAL_EQUAL : TokenType::EQUAL, start, CurIdx, curLine, curCol); break;
    case '>': addToken(match('=') ? TokenType::GREATER_THAN_EQUAL : TokenType::GREATER_THAN, start, CurIdx, curLine, curCol); break;
    case '{': addToken(TokenType::OPEN_BRACE, start, CurIdx, curLine, curCol); break;
    case '}': addToken(TokenType::CLOSE_BRACE, start, CurIdx, curLine, curCol); break;

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
  uint32_t start = CurIdx, curLine = LineNum, curCol = ColNum;
  if (CurIdx > 0) {
    start--;
  }
  while (isalnum(getCurChar()) || getCurChar() == '_') {
    advance();
  }

  uint32_t end = CurIdx;
  auto found = Llama::keywords.find(Input.substr(start, end - start));

  if (found != Llama::keywords.end()) {
    addToken(found->second, start, end, curLine, curCol);
    if (found->second == TokenType::ENCODINGS) { parseEncodingsList(); }
  }
  else {
    addToken(TokenType::IDENTIFIER, start, end, curLine, curCol);
  }
}

void LlamaLexer::parseString() {
  uint32_t start = CurIdx, curLine = LineNum, curCol = ColNum;
  while(getCurChar() != '"' && !isAtEnd()) {
    advance();
  }
  if (isAtEnd()) {
    throw UnexpectedInputError("Unterminated string");
  }
  uint32_t end = CurIdx;
  advance(); // consume closing quote
  addToken(TokenType::DOUBLE_QUOTED_STRING, curLine, curCol, curLine, curCol);
}

void LlamaLexer::parseNumber() {
  uint32_t start = CurIdx, curLine = LineNum, curCol = ColNum;
  if (CurIdx > 0) {
    start--;
  }
  while (isdigit(getCurChar())) {
    advance();
  }

  uint32_t end = CurIdx;
  addToken(TokenType::NUMBER, start, end, curLine, curCol);
}

void LlamaLexer::parseEncodingsList() {
  uint32_t curLine = LineNum, curCol = ColNum;
  if (match('=')) {
    addToken(TokenType::EQUAL, CurIdx-1, CurIdx, curLine, curCol);
    uint32_t start = CurIdx;
    while (!std::isspace(getCurChar()) && !isAtEnd()) {
      advance();
    }
    uint32_t end = CurIdx;
    addToken(TokenType::ENCODINGS_LIST, start, end, curLine, curCol);
  }
  else {
    throw UnexpectedInputError("Expected '=' after 'encodings'");
  }
}

void LlamaLexer::addToken(TokenType type, uint32_t start, uint32_t end, uint32_t lineNum, uint32_t colNum) {
  Tokens.push_back(Token(type, start, end, lineNum, colNum));
}

char LlamaLexer::advance() {
  char curChar = getCurChar();
  ++CurIdx;
  ++ColNum;
  return curChar;
}

bool LlamaLexer::match(char expected) {
  if (isAtEnd() || Input.at(CurIdx) != expected) {
    return false;
  }

  advance();
  return true;
}

std::string_view LlamaLexer::getLexeme(int idx) const {
  return std::string_view(Input).substr(Tokens.at(idx).Start, Tokens.at(idx).End - Tokens.at(idx).Start);
}

char LlamaLexer::getCurChar() const {
  if (CurIdx >= Input.size()) {
    return '\0';
  }
  else {
    return Input.at(CurIdx);
  }
}