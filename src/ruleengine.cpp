#include "ruleengine.h"
#include "llamaduck.h"
#include "rulereader.h"

LlamaRuleEngine::LlamaRuleEngine() 
  : Reader(),
    Qb(Reader.getParser()),
    RuleMatches(std::make_unique<RuleMatchBatch>()),
    RuleRecs(std::make_unique<RuleRecBatch>()) {}

void LlamaRuleEngine::writeRulesToDb(LlamaDBConnection& dbConn) {
  if (Reader.getRules().empty()) {
    return;
  }
  duckdb_result result;
  std::string hits_query;
  // Pre-allocate: ~200 chars per rule SQL + INSERT prefix + semicolon
  hits_query.reserve(Reader.getRules().size() * 220 + 50);
  hits_query = "INSERT INTO rule_hits ";
  const std::string sqlUnion(" UNION ");

  for (const Rule& rule : Reader.getRules()) {
    RuleRecs->add(RuleRec{rule.getHash(Reader.getParser()).to_string(), std::string(rule.Name)});
    hits_query += "(";
    hits_query += Qb.buildSqlQuery(rule);
    hits_query += ")";
    hits_query += sqlUnion;
  }
  hits_query.erase(hits_query.size() - sqlUnion.size()); // remove last UNION
  hits_query += ";";

  LlamaDBAppender appender(dbConn.get(), "rules");
  RuleRecs->copyToDB(appender.get());
  appender.flush();
  
  auto state = duckdb_query(dbConn.get(), hits_query.c_str(), &result);
  THROW_IF(state == DuckDBError, "Error inserting into rule matches table");
}

void LlamaRuleEngine::createTables(LlamaDBConnection& dbConn) {
  DBType<RuleRec> ruleRec;
  THROW_IF(!ruleRec.createTable(dbConn.get(), "rules"), "Error creating rule table");
  DBType<RuleMatch> ruleMatch;
  THROW_IF(!ruleMatch.createTable(dbConn.get(), "rule_hits"), "Error creating rule hits table");
  DBType<SearchHit> searchHit;
  THROW_IF(!searchHit.createTable(dbConn.get(), "search_hits"), "Error creating search hit table");
}

LgFsmHolder LlamaRuleEngine::buildFsm() {
  LgFsmHolder fsm;
  FieldHash h;
  for (const Rule& rule : Reader.getRules()) {
    h = rule.getHash(Reader.getParser());
    for (const auto& pPair : rule.Grep.Patterns.Patterns) {
      fsm.addPatterns(pPair, Reader.getParser(), h.to_string(), PatternToRuleId);
    }
  }
  return fsm;
}

bool LlamaRuleEngine::read(const std::string& input, const std::string& source) {
  // Make a copy of the input and save as member to ensure input string lifetime
  // (since we're passing around `string_view`s)
  Input = input;
  return Reader.read(Input, source);
}

uint64_t LlamaRuleEngine::numRulesRead() {
  return Reader.getRules().size();
}

void LlamaRuleEngine::addRuleMatch(const RuleMatch& match) {
  RuleMatches->add(match);
}

void LlamaRuleEngine::addRuleRec(const RuleRec& ruleRec) {
  RuleRecs->add(ruleRec);
}

void LlamaRuleEngine::flush(LlamaDBAppender& appender) {
  RuleMatches->copyToDB(appender.get());
  appender.flush();
}