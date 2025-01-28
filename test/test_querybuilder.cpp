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
  REQUIRE(qb.buildSqlQuery(rules.at(0)) == "SELECT '" + rules.at(0).getHash(parser).to_string() + "', Path, Name, Addr FROM dirent, inode WHERE dirent.Metaaddr == inode.Addr");
}

TEST_CASE("buildSqlQueryFromRuleWithOneNumberFileMetadataCondition") {
  std::string input = "rule MyRule { file_metadata: filesize == 30000 }";
  LlamaParser parser(input, LlamaLexer::getTokens(input));
  QueryBuilder qb(parser);
  std::vector<Rule> rules = parser.parseRules({0});
  REQUIRE(rules.at(0).Name == "MyRule");
  REQUIRE(qb.buildSqlQuery(rules.at(0)) == "SELECT '" + rules.at(0).getHash(parser).to_string() + "', Path, Name, Addr FROM dirent, inode WHERE dirent.Metaaddr == inode.Addr AND Filesize == 30000");
}

TEST_CASE("buildSqlQueryFromRuleWithOneStringFileMetadataCondition") {
  std::string input = "rule MyRule { file_metadata: created > \"2023-05-04\" }";
  LlamaParser parser(input, LlamaLexer::getTokens(input));
  QueryBuilder qb(parser);
  std::vector<Rule> rules = parser.parseRules({0});
  REQUIRE(rules.at(0).Name == "MyRule");
  REQUIRE(qb.buildSqlQuery(rules.at(0)) == "SELECT '" + rules.at(0).getHash(parser).to_string() + "', Path, Name, Addr FROM dirent, inode WHERE dirent.Metaaddr == inode.Addr AND Created > '2023-05-04'");
}

TEST_CASE("buildSqlQueryFromRuleWithCompoundFileMetadataDef") {
  std::string input = "rule MyRule { file_metadata: filesize == 123456 or created > \"2023-05-04\" and modified < \"2023-05-06\" and filename == \"test\" and filepath == \"test\" }";
  LlamaParser parser(input, LlamaLexer::getTokens(input));
  QueryBuilder qb(parser);
  std::vector<Rule> rules = parser.parseRules({0});
  REQUIRE(rules.at(0).Name == "MyRule");
  REQUIRE(qb.buildSqlQuery(rules.at(0)) == "SELECT '" + rules.at(0).getHash(parser).to_string() + "', Path, Name, Addr FROM dirent, inode WHERE dirent.Metaaddr == inode.Addr AND (Filesize == 123456 OR (((Created > '2023-05-04' AND Modified < '2023-05-06') AND Name == 'test') AND Path == 'test'))");
}

// TEST_CASE("buildSqlQueryFromRuleWithAnyFunc") {
//   std::string input = "rule MyRule { grep: patterns: a = \"foo\" b = \"bar\" condition: any() }";
//   LlamaParser parser(input, LlamaLexer::getTokens(input));
//   QueryBuilder qb(parser);
//   std::vector<Rule> rules = parser.parseRules({0});
//   std::string expectedQuery = "SELECT '" + rules.at(0).getHash(parser).to_string() + "', Addr "
//                               "FROM inode, hash, search_hits "
//                               "WHERE hash.MetaAddr == inode.Addr "
//                               "AND hash.Blake3 == search_hits.file_hash "
//                               "AND ANY(SELECT pattern from search_hits WHERE pattern == 'foo' AND rule_id == '" + rules.at(0).getHash(parser).to_string() + "') "
//                               "OR ANY(SELECT pattern from search_hits WHERE pattern == 'bar' AND rule_id == '" + rules.at(0).getHash(parser).to_string() + "'))";
// }

// TEST_CASE("buildSqlQueryFromRuleWithAllFunc") {
//   std::string input = "rule MyRule { grep: patterns: a = \"foo\" b = \"bar\" condition: all() }";
//   LlamaParser parser(input, LlamaLexer::getTokens(input));
//   QueryBuilder qb(parser);
//   std::vector<Rule> rules = parser.parseRules({0});
//   std::string expectedQuery = "SELECT '" + rules.at(0).getHash(parser).to_string() + "', Path, Name, Addr "
//                               "FROM dirent, hash, search_hits "
//                               "WHERE hash.MetaAddr == dirent.Metaaddr "
//                               "AND hash.Blake3 == search_hits.file_hash "
//                               "AND EXISTS(FROM search_hits WHERE rule_id == '" + rules.at(0).getHash(parser).to_string() + "' AND pattern == 'foo')"
//                               "AND EXISTS (FROM search_hits WHERE rule_id == '" + rules.at(0).getHash(parser).to_string() + "' AND pattern == 'bar'))";
// }

// TEST_CASE("buildSqlQueryFromRuleWithCountFunc") {
//   std::string input = "rule MyRule { grep: patterns: a = \"foo\" b = \"bar\" condition: count(a) == 5 }";
//   LlamaParser parser(input, LlamaLexer::getTokens(input));
//   QueryBuilder qb(parser);
//   std::vector<Rule> rules = parser.parseRules({0});
//   std::string expectedQuery = "SELECT '" + rules.at(0).getHash(parser).to_string() + "', Path, Name, Addr "
//                               "FROM dirent, inode, hash, search_hits "
//                               "WHERE dirent.Metaaddr == inode.Addr "
//                               "AND hash.MetaAddr == dirent.Metaaddr "
//                               "AND hash.Blake3 == search_hits.file_hash "
//                               "AND 5 == (SELECT count(*) FROM search_hits WHERE rule_id == '" + rules.at(0).getHash(parser).to_string() + "' AND pattern == 'r-alloc' AND hash.Blake3 == file_hash)";;
// }

// TEST_CASE("buildSqlQueryFromRuleWithLengthFunc") {
//   std::string input = "rule MyRule { grep: patterns: a = \"foo\" condition: length(a) == 5 }";
//   LlamaParser parser(input, LlamaLexer::getTokens(input));
//   QueryBuilder qb(parser);
//   std::vector<Rule> rules = parser.parseRules({0});
//   std::string expectedQuery = "SELECT '" + rules.at(0).getHash(parser).to_string() + "', Path, Name, Addr "
//                               "FROM dirent, inode, hash, search_hits "
//                               "WHERE dirent.Metaaddr == inode.Addr "
//                               "AND hash.MetaAddr == dirent.Metaaddr "
//                               "AND hash.Blake3 == search_hits.file_hash "
//                               "AND EXISTS (FROM search_hits WHERE rule_id == '" + rules.at(0).getHash(parser).to_string() + "' AND pattern == 'foo' AND length == 5)";
// }

/*
count

select distinct 'dfc3147f51078e8460fdd030fe2db615789835e85871ffb10929ac409037a492', Addr 
from inode, hash, search_hits 
where inode.Addr == hash.MetaAddr 
and hash.blake3 in 
  (select distinct file_hash from 
    (select file_hash, count(file_hash) as count, pattern, length
    from search_hits 
    where rule_id == 'dfc3147f51078e8460fdd030fe2db615789835e85871ffb10929ac409037a492' 
    group by file_hash, pattern
    having (count >= 2 and pattern == 'r-unalloc')));

(count(a) > 2 && length(a) > 5) or (count(b) > 3 && length(b) > 6)

length
any
all
offset
count_has_hits
*/