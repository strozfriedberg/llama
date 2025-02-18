#pragma once

#include <string>
#include <vector>

#include "parser.h"
#include "lexer.h"

class RuleReader {
public:
  bool read(const std::string& input, const std::string& source);
  void clear() { Rules.clear(); LastError.clear(); Parser.clear(); }

  const std::vector<Rule>& getRules() const { return Rules; }
  const std::string& getLastError() const { return LastError; }
  const LlamaParser& getParser() const { return Parser; }
  const LlamaLexer& getLexer() const { return Lexer; }

private:
  std::vector<Rule> Rules;
  std::string LastError;
  LlamaParser Parser;
  LlamaLexer Lexer;
};
