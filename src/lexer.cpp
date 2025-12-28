#include "lexer.h"

#include <bitset>
#include <cstring>

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
    try {
      scanToken();
    }
    catch (const UnexpectedInputError& e) {
      Errors.push_back(e);
    }
  }
  addToken(LlamaTokenType::END_OF_FILE, CurIdx, CurIdx+1, Pos);
}

void LlamaLexer::scanToken() {
  if (isAtEnd()) {
    throw std::out_of_range("Lexer attempted to scan past the end of input stream.");
  }
  const uint64_t start = CurIdx;
  const LineCol pos(Pos);
  const uint8_t c = advance();
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
        throw UnexpectedInputError("Unexpected input character: !", pos);
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
        addToken(LlamaTokenType::UNRECOGNIZED, start, CurIdx, pos);
      }
  }
}

namespace {
  LlamaTokenType keywordOrID(const std::string_view& identifier) {
    // using the first character results in fewer collisions than string length
    // hardcoding here is quite a bit faster than unordered_map lookup
    auto first = identifier[0];
    switch (first) {
      case 'a':
        if (identifier == "and") return LlamaTokenType::AND;
        break;
      case 'b':
        if (identifier == "blake3") return LlamaTokenType::BLAKE3;
        break;
      case 'c':
        if (identifier == "condition") return LlamaTokenType::CONDITION;
        break;
      case 'e':
        if (identifier == "encodings") return LlamaTokenType::ENCODINGS;
        break;
      case 'f':
        if (identifier == "file_metadata") return LlamaTokenType::FILE_METADATA;
        if (identifier == "fixed") return LlamaTokenType::FIXED;
        break;
      case 'g':
        if (identifier == "grep") return LlamaTokenType::GREP;
        break;
      case 'h':
        if (identifier == "hash") return LlamaTokenType::HASH;
        break;
      case 'm':
        if (identifier == "meta") return LlamaTokenType::META;
        if (identifier == "md5") return LlamaTokenType::MD5;
        break;
      case 'n':
        if (identifier == "nocase") return LlamaTokenType::NOCASE;
        break;
      case 'o':
        if (identifier == "or") return LlamaTokenType::OR;
        break;
      case 'p':
        if (identifier == "patterns") return LlamaTokenType::PATTERNS;
        break;
      case 'r':
        if (identifier == "rule") return LlamaTokenType::RULE;
        break;
      case 's':
        if (identifier == "sha1") return LlamaTokenType::SHA1;
        if (identifier == "sha256") return LlamaTokenType::SHA256;
        if (identifier == "signature") return LlamaTokenType::SIGNATURE;
        break;
      default:
        break;
    }
    return LlamaTokenType::IDENTIFIER;
  }
}

void LlamaLexer::parseIdentifier(LineCol pos) {
  uint64_t start = CurIdx;
  if (CurIdx > 0) {
    start--;
  }
  while (!isAtEnd() && IdentifierChars[curChar()]) {
    advance();
  }
  uint64_t end = CurIdx;
  std::string_view identifier(Input.data() + start, end - start);

  LlamaTokenType type = keywordOrID(identifier);
  addToken(type, start, end, pos);
  if (type == LlamaTokenType::RULE) {
    RuleIndices.push_back(Tokens.size() - 1);
  }
}

void LlamaLexer::parseString(LineCol pos) {
  const uint64_t start = CurIdx;
  const char* cur = Input.data() + start;
  const char* closeQuote = nullptr;
  const char* backslash = nullptr;
  do {
    closeQuote = static_cast<const char*>(std::memchr(cur, '"', Input.end() - cur));
    if (closeQuote == nullptr) {
      CurIdx = InputSize;
      throw UnexpectedInputError("Unterminated string", pos);
    }
    backslash  = static_cast<const char*>(std::memchr(cur, '\\', closeQuote - cur));
    if (backslash != nullptr) {
      // we've got to deal with escaping, do a loop from start of backslash
      do {
        auto lookahead = backslash + 1;
        if (*lookahead == '"') {
          cur = lookahead + 1; // escape quote, find next quote
          break;               // repeat outer do-loop
        }
        else {
          ++lookahead; // skip escaped char to find next backslash
          backslash = static_cast<const char*>(std::memchr(lookahead, '\\', closeQuote - lookahead));
        }
      } while (backslash != nullptr && backslash < closeQuote);
    }
  } while (backslash != nullptr && backslash < cur); // even if cur is end of string, loop for exception throw

  const uint64_t end = closeQuote - Input.data();
  CurIdx = end + 1; // move past closing quote
  addToken(LlamaTokenType::DOUBLE_QUOTED_STRING, start, end, pos);
}

void LlamaLexer::parseNumber(LineCol pos) {
  uint64_t start = CurIdx;
  if (CurIdx > 0) {
    start--;
  }
  while (!isAtEnd() && isdigit(curChar())) {
    advance();
  }
  uint64_t end = CurIdx;
  addToken(LlamaTokenType::NUMBER, start, end, pos);
}

void LlamaLexer::parseSingleLineComment() {
  while (!isAtEnd() && curChar() != '\n') {
    advance();
  }
}

void LlamaLexer::parseMultiLineComment(LineCol pos) {
  while (!isAtEnd() && curChar() != '*') {
    if (curChar() == '\n') {
      Pos.LineNum++;
      Pos.ColNum = 0;
    }
    advance();
  }
  if (isAtEnd()) {
    throw UnexpectedInputError("Unterminated multi-line comment", pos);
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
