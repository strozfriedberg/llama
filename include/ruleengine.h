#pragma once

#include "fsm.h"

class RuleReader;
class LlamaDBConnection;

class RuleEngine {
public:
  void writeRulesToDb(const RuleReader& reader, LlamaDBConnection& dbConn);
  void createTables(LlamaDBConnection& dbConn);

  LgFsmHolder getFsm(const RuleReader& reader);

  const std::vector<std::string>& patternToRuleId() const { return PatternToRuleId; }
private:
  std::vector<std::string> PatternToRuleId;
};
