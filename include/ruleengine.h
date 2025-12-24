#pragma once

#include "fsm.h"
#include "llamabatch.h"
#include "querybuilder.h"
#include "rulereader.h"

class LlamaDBConnection;

class LlamaRuleEngine {
public:
  LlamaRuleEngine();
  void writeRulesToDb(LlamaDBConnection& dbConn);
  void createTables(LlamaDBConnection& dbConn);

  LgFsmHolder buildFsm();

  bool read(const std::string& input, const std::string& source);
  uint64_t numRulesRead();
  void addRuleMatch(const RuleMatch& match);
  void addRuleRec(const RuleRec& ruleRec);
  void flush(LlamaDBAppender& appender);

  const std::vector<std::string>& patternToRuleId() const { return PatternToRuleId; }
  void setPatternToRuleId(const std::vector<std::string>& patternToRuleId) { PatternToRuleId = patternToRuleId; }
  const RuleReader& getReader() const { return Reader; }
private:
  std::vector<std::string> PatternToRuleId;
  std::string Input;
  RuleReader Reader;
  QueryBuilder Qb;

  std::unique_ptr<RuleMatchBatch> RuleMatches;
  std::unique_ptr<RuleRecBatch> RuleRecs;
};
