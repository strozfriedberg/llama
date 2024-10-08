#include "lexer.h"

void print(std::string s) {
  std::cout << s << std::endl;
}

void LlamaLexer::scanTokens() {
  while (!isAtEnd()) {
    scanToken();
  }
  addToken(TokenType::END_OF_FILE, CurIdx, CurIdx+1, Pos);
}

void LlamaLexer::scanToken() {
  uint64_t start = CurIdx;
  LineCol pos(Pos);
  char c = advance();
  switch(c) {
    case '\t': break;
    case '\n': Pos.ColNum = 1; Pos.LineNum++; break;
    case '\r':
    case ' ': break;

    case '!' : {
      if (match('=')) {
        addToken(TokenType::NOT_EQUAL, start, CurIdx, pos);
      }
      else {
        throw UnexpectedInputError("Unexpected input character: ! at ", pos);
      }
      break;
    }

    case '"': parseString(pos); break;

    case '(': addToken(TokenType::OPEN_PAREN, start, CurIdx, pos); break;
    case ')': addToken(TokenType::CLOSE_PAREN, start, CurIdx, pos); break;
    case ',': addToken(TokenType::COMMA, start, CurIdx, pos); break;

    case '/': {
      if (match('/')) {
        parseSingleLineComment();
      }
      else if (match('*')) {
        parseMultiLineComment(pos);
      }
      else {
        throw UnexpectedInputError("Unexpected input character: / at ", pos);
      }
      break;
    }

    case ':': addToken(TokenType::COLON, start, CurIdx, pos); break;
    case '<': addToken(match('=') ? TokenType::LESS_THAN_EQUAL : TokenType::LESS_THAN, start, CurIdx, pos); break;
    case '=': addToken(match('=') ? TokenType::EQUAL_EQUAL : TokenType::EQUAL, start, CurIdx, pos); break;
    case '>': addToken(match('=') ? TokenType::GREATER_THAN_EQUAL : TokenType::GREATER_THAN, start, CurIdx, pos); break;
    case '{': addToken(TokenType::OPEN_BRACE, start, CurIdx, pos); break;
    case '}': addToken(TokenType::CLOSE_BRACE, start, CurIdx, pos); break;

    default:
      if (isdigit(c)) {
        parseNumber(pos);
      }
      else if (isalnum(c)) {
        parseIdentifier(pos);
      }
      else {
        throw UnexpectedInputError("Unexpected input character at ", pos);
      }
  }
}

void LlamaLexer::parseIdentifier(LineCol pos) {
  uint64_t start = CurIdx;
  if (CurIdx > 0) {
    start--;
  }
  while (isalnum(getCurChar()) || getCurChar() == '_' || getCurChar() == '-') {
    advance();
  }

  uint64_t end = CurIdx;
  auto found = llama::keywords.find(Input.substr(start, end - start));

  if (found != llama::keywords.end()) {
    addToken(found->second, start, end, pos);
  }
  else {
    addToken(TokenType::IDENTIFIER, start, end, pos);
  }
}

void LlamaLexer::parseString(LineCol pos) {
  uint64_t start = CurIdx;
  while(getCurChar() != '"' && !isAtEnd()) {
    if (getCurChar() == '\\') {
      advance();
    }
    advance();
  }
  if (isAtEnd()) {
    throw UnexpectedInputError("Unterminated string at ", pos);
  }
  uint64_t end = CurIdx;
  advance(); // consume closing quote
  addToken(TokenType::DOUBLE_QUOTED_STRING, start, end, pos);
}

void LlamaLexer::parseNumber(LineCol pos) {
  uint64_t start = CurIdx;
  if (CurIdx > 0) {
    start--;
  }
  while (isdigit(getCurChar())) {
    advance();
  }

  uint64_t end = CurIdx;
  addToken(TokenType::NUMBER, start, end, pos);
}

void LlamaLexer::parseSingleLineComment() {
  while (getCurChar() != '\n' && !isAtEnd()) {
    advance();
  }
}

void LlamaLexer::parseMultiLineComment(LineCol pos) {
  while (getCurChar() != '*' && !isAtEnd()) {
    if (getCurChar() == '\n') {
      Pos.LineNum++;
      Pos.ColNum = 0;
    }
    advance();
  }
  if (isAtEnd()) {
    throw UnexpectedInputError("Unterminated multi-line comment at ", pos);
  }
  if (peek() == '/') {
    advance(); // consume *
    advance(); // consume /
  }
  else {
    advance();
    parseMultiLineComment(pos);
  }
}

void LlamaLexer::addToken(TokenType type, uint64_t start, uint64_t end, LineCol pos) {
  Tokens.push_back(Token(type, start, end, pos));
}

char LlamaLexer::advance() {
  char curChar = getCurChar();
  ++CurIdx;
  ++Pos.ColNum;
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
  if (isAtEnd()) {
    return '\0';
  }
  else {
    return Input.at(CurIdx);
  }
}
