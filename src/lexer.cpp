#include "lexer.h"

namespace {
  std::bitset<256> initIdentifierChars() {
    std::bitset<256> b;
    b.reset();
    for (int c = 0; c < 256; ++c) {
      if (isalnum(c) || c == '_' || c == '-') {
        b.set(c);
      }
    }
    return b;
  }
  static const std::bitset<256> IdentifierChars = initIdentifierChars();
}

void LlamaLexer::scanTokens() {
  // Estimate final size of the token vector to eliminate array doubling.
  // Set to length of input since there can't possibly be more tokens than characters.
  Tokens.reserve(InputSize);
  while (!isAtEnd()) {
    scanToken();
  }
  addToken(LlamaTokenType::END_OF_FILE, CurIdx, CurIdx+1, Pos);
}

void LlamaLexer::scanToken() {
  const uint64_t start = CurIdx;
  const LineCol pos(Pos);
  const char c = advance();
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
      else if (isalpha(c)) {
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
  char c = getCurChar();
  while (IdentifierChars[c]) {
    advance();
    c = getCurChar();
  }

  uint64_t end = CurIdx;
  auto found = LlamaKeywords.find(Input.substr(start, end - start));

  if (found != LlamaKeywords.end()) {
    addToken(found->second, start, end, pos);
    if (found->second == LlamaTokenType::RULE) {
      ++RuleCount;
      RuleIndices.push_back(Tokens.size() - 1);
    }
  }
  else {
    addToken(LlamaTokenType::IDENTIFIER, start, end, pos);
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
  advance(); // consume closing quote
  uint64_t end = CurIdx - 1;
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

bool LlamaLexer::match(char expected) {
  if (isAtEnd() || Input[CurIdx] != expected) {
    return false;
  }

  advance();
  return true;
}
