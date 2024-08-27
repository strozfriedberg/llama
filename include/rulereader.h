#pragma once

#include <string>
#include <vector>

#include "lexer.h"
#include "parser.h"

class RuleReader {
public:
  int read(const std::string& input);

  const std::vector<Rule>& getRules() const { return Rules; }
  const std::string& getLastError() const { return LastError; }

private:
  std::vector<Rule> Rules;
  std::string LastError;
};

int RuleReader::read(const std::string& input) {
  LlamaLexer lexer(input);
  try {
    lexer.scanTokens();
    LlamaParser parser(input, lexer.getTokens());
    std::vector<Rule> rules = parser.parseRules();
    Rules.reserve(Rules.size() + rules.size());
    Rules.insert(Rules.end(), rules.begin(), rules.end());
  }
  catch (UnexpectedInputError& e) {
    LastError = e.messageWithPos();
    return -1;
  }

  return Rules.size();
}

