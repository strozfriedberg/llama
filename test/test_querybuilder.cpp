#include <catch2/catch_test_macros.hpp>

#include "lexer.h"
#include "querybuilder.h"

TEST_CASE("buildPropertyNodeSqlClause") {
  std::string input("rule MyRule { file_metadata: filesize == 123456 }");
  LlamaParser parser(input, LlamaLexer::getTokens(input));
  QueryBuilder qb(parser);
  PropertyNode n({Property{5, 6, 7}});
  std::shared_ptr<PropertyNode> pn = std::make_shared<PropertyNode>(n);
  std::string expected = "Filesize == 123456"; 
  REQUIRE(qb.buildSqlClause(pn) == expected);
}

TEST_CASE("buildSqlQueryFromRule") {
  std::string input("rule MyRule { }");
  LlamaParser parser(input, LlamaLexer::getTokens(input));
  QueryBuilder qb(parser);
  std::vector<Rule> rules = parser.parseRules({0});
  REQUIRE(rules.at(0).Name == "MyRule");
  REQUIRE(rules.at(0).getSqlQuery(parser) == "SELECT '" + rules.at(0).getHash(parser).to_string() + "', Path, Name, Addr FROM dirent, inode WHERE dirent.Metaaddr == inode.Addr");
}

TEST_CASE("buildSqlQueryFromRuleWithOneNumberFileMetadataCondition") {
  std::string input = "rule MyRule { file_metadata: filesize == 30000 }";
  LlamaParser parser(input, LlamaLexer::getTokens(input));
  QueryBuilder qb(parser);
  std::vector<Rule> rules = parser.parseRules({0});
  REQUIRE(rules.at(0).Name == "MyRule");
  REQUIRE(rules.at(0).getSqlQuery(parser) == "SELECT '" + rules.at(0).getHash(parser).to_string() + "', Path, Name, Addr FROM dirent, inode WHERE dirent.Metaaddr == inode.Addr AND Filesize == 30000");
}

TEST_CASE("buildSqlQueryFromRuleWithOneStringFileMetadataCondition") {
  std::string input = "rule MyRule { file_metadata: created > \"2023-05-04\" }";
  LlamaParser parser(input, LlamaLexer::getTokens(input));
  QueryBuilder qb(parser);
  std::vector<Rule> rules = parser.parseRules({0});
  REQUIRE(rules.at(0).Name == "MyRule");
  REQUIRE(rules.at(0).getSqlQuery(parser) == "SELECT '" + rules.at(0).getHash(parser).to_string() + "', Path, Name, Addr FROM dirent, inode WHERE dirent.Metaaddr == inode.Addr AND Created > '2023-05-04'");
}

TEST_CASE("buildSqlQueryFromRuleWithCompoundFileMetadataDef") {
  std::string input = "rule MyRule { file_metadata: filesize == 123456 or created > \"2023-05-04\" and modified < \"2023-05-06\" and filename == \"test\" and filepath == \"test\" }";
  LlamaParser parser(input, LlamaLexer::getTokens(input));
  QueryBuilder qb(parser);
  std::vector<Rule> rules = parser.parseRules({0});
  REQUIRE(rules.at(0).Name == "MyRule");
  REQUIRE(rules.at(0).getSqlQuery(parser) == "SELECT '" + rules.at(0).getHash(parser).to_string() + "', Path, Name, Addr FROM dirent, inode WHERE dirent.Metaaddr == inode.Addr AND (Filesize == 123456 OR (((Created > '2023-05-04' AND Modified < '2023-05-06') AND Name == 'test') AND Path == 'test'))");
}