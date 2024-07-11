#include <catch2/catch_test_macros.hpp>

#include "lexer.h"

class LlamaParser {
public:
  LlamaParser(std::vector<Token> tokens) : tokens(tokens) {}
  std::vector<Token> tokens;
};

TEST_CASE("LlamaParser") {
  std::vector<Token> tokens;
  LlamaParser parser(tokens);
}