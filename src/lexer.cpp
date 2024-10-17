#include "lexer.h"

void LlamaLexer::scanTokens() {
  // Estimate final size of the token vector to eliminate array doubling.
  // Set to length of input since there can't possibly be more tokens than characters.
  Tokens.reserve(Input.length());
  while (!isAtEnd()) {
    scanToken();
  }
  addToken(LlamaTokenType::END_OF_FILE, CurIdx, CurIdx+1, Pos);
}

void LlamaLexer::scanToken() {
  uint64_t start = CurIdx;
  LineCol pos(Pos);
  char c = advance();
  LlamaTokenType op;
  switch(c) {
    case '\t': break;
    case '\n': Pos.ColNum = 1; Pos.LineNum++; break;
    case '\r':
    case ' ': break;

    case '!' : {
      if (match('=')) {
        addToken(LlamaTokenType::NOT_EQUAL, start, CurIdx, pos);
      }
      else {
        throw UnexpectedInputError("Unexpected input character: ! at ", pos);
      }
      break;
    }

    case '"': parseString(pos); break;

    case '(': addToken(LlamaTokenType::OPEN_PAREN, start, CurIdx, pos); break;
    case ')': addToken(LlamaTokenType::CLOSE_PAREN, start, CurIdx, pos); break;
    case ',': addToken(LlamaTokenType::COMMA, start, CurIdx, pos); break;

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

    case ':': addToken(LlamaTokenType::COLON, start, CurIdx, pos); break;
    case '<': op = match('=') ? LlamaTokenType::LESS_THAN_EQUAL : LlamaTokenType::LESS_THAN; addToken(op, start, CurIdx, pos); break;
    case '=': op = match('=') ? LlamaTokenType::EQUAL_EQUAL : LlamaTokenType::EQUAL; addToken(op, start, CurIdx, pos); break;
    case '>': op = match('=') ? LlamaTokenType::GREATER_THAN_EQUAL : LlamaTokenType::GREATER_THAN; addToken(op, start, CurIdx, pos); break;
    case '{': addToken(LlamaTokenType::OPEN_BRACE, start, CurIdx, pos); break;
    case '}': addToken(LlamaTokenType::CLOSE_BRACE, start, CurIdx, pos); break;

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
  auto found = LlamaKeywords.find(Input.substr(start, end - start));

  if (found != LlamaKeywords.end()) {
    addToken(found->second, start, end, pos);
    if (found->second == LlamaTokenType::RULE) ++RuleCount;
  }
  else {
    addToken(LlamaTokenType::IDENTIFIER, start, end, pos);
  }
}

void LlamaLexer::parseString(LineCol pos) {
  uint64_t start = CurIdx - 1;
  while(getCurChar() != '"' && !isAtEnd()) {
    if (getCurChar() == '\\') {
      advance();
    }
    advance();
  }
  if (isAtEnd()) {
    throw UnexpectedInputError("Unterminated string at ", pos);
  }
  advance(); // consume closing quote
  uint64_t end = CurIdx;
  addToken(LlamaTokenType::DOUBLE_QUOTED_STRING, start, end, pos);
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
  addToken(LlamaTokenType::NUMBER, start, end, pos);
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

void LlamaLexer::addToken(LlamaTokenType type, uint64_t start, uint64_t end, LineCol pos) {
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

char LlamaLexer::getCurChar() const {
  if (isAtEnd()) {
    return '\0';
  }
  else {
    return Input.at(CurIdx);
  }
}
