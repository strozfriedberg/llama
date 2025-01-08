#include <catch2/catch_test_macros.hpp>

#include "lexer.h"
#include "querybuilder.h"

TEST_CASE("getPropertyNodeSqlClause") {
  std::string input("rule MyRule { file_metadata: filesize == 123456 }");
  LlamaParser parser(input, LlamaLexer::getTokens(input));
  QueryBuilder qb(parser);
  PropertyNode n({Property{5, 6, 7}});
  std::shared_ptr<PropertyNode> pn = std::make_shared<PropertyNode>(n);
  std::string expected = "Filesize == 123456"; 
  REQUIRE(qb.buildSqlClause(pn) == expected);
}