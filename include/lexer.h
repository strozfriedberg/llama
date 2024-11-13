#pragma once

#include <bitset>
#include <cctype>
#include <cstdint>
#include <iostream>
#include <regex>
#include <string_view>
#include <vector>

#include "token.h"

class LlamaLexer {
public:
  LlamaLexer(const std::string& input) : CurIdx(0), RuleCount(0), InputSize(input.size()), Input(input) {};

  void scanTokens();
  void scanToken();

  void parseIdentifier(LineCol pos);
  void parseString(LineCol pos);
  void parseNumber(LineCol pos);
  void parseSingleLineComment();
  void parseMultiLineComment(LineCol pos);

  void addToken(LlamaTokenType type, uint64_t start, uint64_t end, LineCol pos) {
    Tokens.emplace_back(type, Input.substr(start, end - start), pos);
  }

  char advance() { ++Pos.ColNum; return Input[CurIdx++]; }

  bool match(char expected);
  bool isAtEnd() const { return CurIdx == InputSize; }

  unsigned char getCurChar() const { return Input[CurIdx]; };
  char peek() const { return isAtEnd() ? '\0' : Input[CurIdx + 1]; }
  const std::vector<Token>& getTokens() const { return Tokens; }
  const std::vector<size_t>& getRuleIndices() const { return RuleIndices; }

  size_t getRuleCount() const { return RuleCount; }

  LineCol Pos = {1, 1};
private:
  size_t              CurIdx;
  size_t              RuleCount;
  size_t              InputSize;
  std::string_view    Input;
  std::vector<Token>  Tokens;
  std::vector<size_t> RuleIndices;
};