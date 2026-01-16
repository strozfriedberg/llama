#include "lexer.h"
#include "rulereader.h"
#include "fieldhasher.h"

bool RuleReader::read(const std::string& input, const std::string&) {
  Lexer.setInput(input);
  Lexer.scanTokens();
  Parser = LlamaParser(input, Lexer.tokens());
  std::vector<Rule> rules = Parser.parseRules(Lexer.ruleIndices());

  // Compute hashes once for all rules
  FieldHasher hasher;
  Hashes.reserve(Hashes.size() + rules.size());
  for (const Rule& rule : rules) {
    Hashes.push_back(rule.getHash(Parser, hasher));
  }

  Rules.reserve(Rules.size() + rules.size());
  Rules.insert(Rules.end(), std::make_move_iterator(rules.begin()), std::make_move_iterator(rules.end()));
  return (Parser.errors().size() + Lexer.errors().size() == 0);
}
