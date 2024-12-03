#include "ruleengine.h"
#include "llamaduck.h"
#include "rulereader.h"
#include "llamabatch.h"

void RuleEngine::writeRulesToDb(const RuleReader& reader, LlamaDBConnection& dbConn) {
  if (reader.getRules().empty()) {
    return;
  }
  duckdb_result result;
  DBBatch<RuleRec> ruleRecBatch;
  std::string hits_query("INSERT INTO rule_hits ");
  const std::string sqlUnion(" UNION ");

  for (const Rule& rule : reader.getRules()) {
    ruleRecBatch.add(RuleRec{rule.getHash(reader.getParser()).to_string(), std::string(rule.Name)});
    hits_query += "(" + rule.getSqlQuery(reader.getParser()) + ")";
    hits_query += sqlUnion;
  }
  hits_query.erase(hits_query.size() - sqlUnion.size()); // remove last UNION
  hits_query += ";";

  LlamaDBAppender appender(dbConn.get(), "rules");
  ruleRecBatch.copyToDB(appender.get());
  appender.flush();
  
  auto state = duckdb_query(dbConn.get(), hits_query.c_str(), &result);
  THROW_IF(state == DuckDBError, "Error inserting into rule matches table");
}

void RuleEngine::createTables(LlamaDBConnection& dbConn) {
  DBType<RuleRec> ruleRec;
  THROW_IF(!ruleRec.createTable(dbConn.get(), "rules"), "Error creating rule table");
  DBType<RuleMatch> ruleMatch;
  THROW_IF(!ruleMatch.createTable(dbConn.get(), "rule_hits"), "Error creating rule hits table");
  DBType<SearchHit> searchHit;
  THROW_IF(!searchHit.createTable(dbConn.get(), "search_hits"), "Error creating search hit table");
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