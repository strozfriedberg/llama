#pragma once

#include "fsm.h"
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

  const std::vector<std::string>& patternToRuleId() const { return PatternToRuleId; }
private:
  std::vector<std::string> PatternToRuleId;
  std::string Input;
  RuleReader Reader;
  QueryBuilder Qb;
};
