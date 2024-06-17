#include <catch2/catch_test_macros.hpp>

#include <vector>

#include <llama_parser.h>

TEST_CASE("ParseBasicRule") {
  char* input = "rule { }";
  LlamaLexer lexer(input);
  REQUIRE(lexer.getTokens().size() == 3);
}