#include <iostream>
#include <fsm.h>
#include <parser.h>

void LgFsmHolder::addPatterns(
  const PatternPair& pair,
  const LlamaParser& parser,
  const std::string& ruleId,
  std::vector<std::string>& patToRuleId
) {
  uint64_t patternIndex = 0;
  if (pair.second.Enc.first == pair.second.Enc.second) {
    // No encodings were defined for the pattern, so parse with ASCII only
    addPattern(PatParser.parse(pair.second), "ASCII", patternIndex);
    patToRuleId.push_back(ruleId);
    ++patternIndex;
  }
  else {
    for (uint64_t i = pair.second.Enc.first; i < pair.second.Enc.second; i += 2) {
      addPattern(PatParser.parse(pair.second), std::string(parser.Tokens[i].Lexeme).c_str(), patternIndex);
      patToRuleId.push_back(ruleId);
      ++patternIndex;
    }
  }
}