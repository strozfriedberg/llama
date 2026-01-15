#include "lexer.h"
#include "rulereader.h"

bool RuleReader::read(const std::string& input, const std::string&) {
  Lexer.setInput(input);
  Lexer.scanTokens();
  Parser = LlamaParser(input, Lexer.tokens());
  std::vector<Rule> rules = Parser.parseRules(Lexer.ruleIndices());
  Rules.reserve(Rules.size() + rules.size());
  Rules.insert(Rules.end(), std::make_move_iterator(rules.begin()), std::make_move_iterator(rules.end()));
  // Rules.insert(Rules.end(), rules.begin(), rules.end());
  return (Parser.errors().size() + Lexer.errors().size() == 0);
}
