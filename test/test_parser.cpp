#include <catch2/catch_test_macros.hpp>

#include "lexer.h"

class LlamaParser {
public:
  LlamaParser(std::vector<Token> tokens) : Tokens(tokens) {}

  std::vector<Token> Tokens;
  uint32_t CurIdx = 0;
};

TEST_CASE("LlamaParser") {
  std::vector<Token> tokens;
  LlamaParser parser(tokens);
}