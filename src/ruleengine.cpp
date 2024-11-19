#include "ruleengine.h"
#include "llamaduck.h"
#include "rulereader.h"

void RuleEngine::writeRulesToDb(const RuleReader& reader, LlamaDBConnection& dbConn) {
  if (reader.getRules().empty()) {
    return;
  }
  duckdb_result result;
  std::string rule_query("INSERT INTO rules VALUES ");
  std::string matches_query("INSERT INTO rule_matches ");
  const std::string sqlUnion(" UNION ");

  for (const Rule& rule : reader.getRules()) {
    rule_query += "('" + rule.getHash(reader.getParser()).to_string() + "', '" + std::string(rule.Name) + "'),";
    matches_query += "(" + rule.getSqlQuery(reader.getParser()) + ")";
    matches_query += sqlUnion;
  }
  matches_query.erase(matches_query.size() - sqlUnion.size()); // remove last UNION
  matches_query += ";";
  rule_query.pop_back(); // remove last comma
  rule_query += ";";

  auto state = duckdb_query(dbConn.get(), rule_query.c_str(), &result);
  THROW_IF(state == DuckDBError, "Error inserting into rule table");
  state = duckdb_query(dbConn.get(), matches_query.c_str(), &result);
  THROW_IF(state == DuckDBError, "Error inserting into rule matches table");
}

void RuleEngine::createTables(LlamaDBConnection& dbConn) {
  duckdb_result result;
  std::string rule_query("CREATE TABLE rules (id VARCHAR, name VARCHAR);");
  std::string matches_query("CREATE TABLE rule_matches (id VARCHAR, path VARCHAR, name VARCHAR, addr UBIGINT);");
  auto state = duckdb_query(dbConn.get(), rule_query.c_str(), &result);
  THROW_IF(state == DuckDBError, "Error creating rule table");
  state = duckdb_query(dbConn.get(), matches_query.c_str(), &result);
  THROW_IF(state == DuckDBError, "Error creating rule matches table");
}

void RuleEngine::createSearchHitTable(LlamaDBConnection& dbConn) {
  duckdb_result result;
  std::string searchHitQuery("CREATE TABLE search_hits (pattern VARCHAR, start_offset UBIGINT, end_offset UBIGINT, rule_id VARCHAR, file_hash VARCHAR, length UBIGINT);");
  auto state = duckdb_query(dbConn.get(), searchHitQuery.c_str(), &result);
  THROW_IF(state == DuckDBError, "Error creating search hit table");
}

LgFsmHolder RuleEngine::getFsm(const RuleReader& reader) {
  LgFsmHolder fsm;
  FieldHash h;
  for (const Rule& rule : reader.getRules()) {
    h = rule.getHash(reader.getParser());
    for (const auto& pPair : rule.Grep.Patterns.Patterns) {
      fsm.addPatterns(pPair, reader.getParser(), h.to_string(), PatternToRuleId);
    }
  }
  return fsm;
}