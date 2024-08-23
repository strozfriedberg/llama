#pragma once

#include <cctype>
#include <cstdint>
#include <iostream>
#include <regex>
#include <string_view>
#include <vector>

#include "token.h"

class LlamaLexer {
public:
  LlamaLexer(const std::string& input) : Input(input), CurIdx(0) {};

  void scanTokens();
  void scanToken();

  void parseIdentifier(LineCol pos);
  void parseString(LineCol pos);
  void parseNumber(LineCol pos);
  void parseSingleLineComment();
  void parseMultiLineComment(LineCol pos);

  void addToken(TokenType type, uint64_t start, uint64_t end, LineCol pos);

  char advance();

  bool match(char expected);
  bool isAtEnd() const { return CurIdx >= Input.size(); }

  std::string_view getLexeme(int idx) const;
  char getCurChar() const;
  char peek() const { return (isAtEnd() || CurIdx + 1 >= Input.size()) ? '\0' : Input.at(CurIdx + 1); }
  const std::vector<Token>& getTokens() const { return Tokens; }

  LineCol Pos = {1, 1};
private:
  const std::string& Input;
  size_t CurIdx;
  std::vector<Token> Tokens;
};