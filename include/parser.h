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
  bool match(TokenTypes... types) { return (match(types) || ...);};
  bool match(TokenType type);

  bool check(TokenType type) const { return peek().Type == type; }
  bool isAtEnd() const { return peek().Type == TokenType::END_OF_FILE; }

  void parseHash();

  std::vector<Token> Tokens;
  uint32_t CurIdx = 0;
};

bool LlamaParser::match(TokenType type) {
  if (check(type)) {
    advance();
    return true;
  }
  return false;
}

void LlamaParser::parseHash() {
  if (!match(TokenType::MD5, TokenType::SHA1, TokenType::SHA256, TokenType::BLAKE3)) {
    throw ParserError("Expected hash type");
  }
}
