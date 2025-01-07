#include <catch2/catch_test_macros.hpp>

#include "lexer.h"
#include "querygenerator.h"

TEST_CASE("getPropertyNodeSqlClause") {
  std::string input("rule MyRule { file_metadata: filesize == 123456 }");
  LlamaParser parser(input, LlamaLexer::getTokens(input));
  QueryGenerator qg(parser);
  PropertyNode pn{Property{5, 6, 7}};
  std::string expected = "Filesize == 123456"; 
  REQUIRE(qg.getSqlClause(pn) == expected);
}