#pragma once

#include <string>
#include <vector>

#include "parser.h"
#include "lexer.h"
#include "fieldhash.h"

class RuleReader {
public:
  bool read(const std::string& input, const std::string& source);
  void clear() { Rules.clear(); Hashes.clear(); Parser.clear(); }

  const std::vector<Rule>& getRules() const { return Rules; }
  const std::vector<FieldHash>& getRuleHashes() const { return Hashes; }
  const LlamaParser& getParser() const { return Parser; }
  const LlamaLexer& getLexer() const { return Lexer; }

private:
  std::vector<Rule> Rules;
  std::vector<FieldHash> Hashes;
  LlamaParser Parser;
  LlamaLexer Lexer;
};
