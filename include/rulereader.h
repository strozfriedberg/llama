#pragma once

#include <string>
#include <vector>

#include "parser.h"

class RuleReader {
public:
  int read(const std::string& input);
  void clear() { Rules.clear(); LastError.clear(); Parser.clear(); }

  const std::vector<Rule>& getRules() const { return Rules; }
  const std::string& getLastError() const { return LastError; }
  const LlamaParser& getParser() const { return Parser; }

private:
  std::vector<Rule> Rules;
  std::string LastError;
  LlamaParser Parser;
};
