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
  LlamaLexer() = default;
  LlamaLexer(const std::string& input) : CurIdx(0), InputSize(input.size()), Input(input) {};

  void setInput(std::string_view input) { clear(); Input = input; InputSize = input.size(); }

  void clear() {
    CurIdx = 0;
    InputSize = 0;
    Tokens.clear();
    Errors.clear();
    RuleIndices.clear();
    Pos.reset();
  }

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

  char curChar() const { return Input[CurIdx]; };

  // Peek at the next char without consuming the current one.
  char peek() const { return isAtEnd() ? '\0' : Input[CurIdx + 1]; }

  const std::vector<Token>& tokens() const { return Tokens; }
  const std::vector<size_t>& ruleIndices() const { return RuleIndices; }
  const std::vector<UnexpectedInputError>& errors() const { return Errors; }

  LineCol Pos = {1, 1};
private:
  size_t                            CurIdx;
  size_t                            InputSize;
  std::string_view                  Input;
  std::vector<Token>                Tokens;
  std::vector<UnexpectedInputError> Errors;
  std::vector<size_t>               RuleIndices;
};