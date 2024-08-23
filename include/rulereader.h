#pragma once

#include <string>
#include <vector>

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
