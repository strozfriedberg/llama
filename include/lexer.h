#include <cctype>
#include <cstdint>
#include <iostream>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "token.h"

class UnexpectedInputError : public std::runtime_error {
public:
  UnexpectedInputError(const std::string& message) : std::runtime_error(message) {}
};

class LlamaLexer {
public:
  LlamaLexer(const std::string& input) : Input(input), CurIdx(0) {};

  void scanTokens();
  void scanToken();

  void parseIdentifier();
  void parseString();
  void parseNumber();
  void parseEncodingsList();

  void addToken(TokenType type, uint32_t start, uint32_t end) { Tokens.push_back(Token(type, start, end)); }

  char advance();

  bool match(char expected);
  bool isAtEnd() const { return CurIdx >= Input.size(); }

  std::string_view getLexeme(int idx) const;
  char getCurChar() const;
  const std::vector<Token>& getTokens() const { return Tokens; }

private:
  const std::string& Input;
  size_t CurIdx;
  std::vector<Token> Tokens;
};