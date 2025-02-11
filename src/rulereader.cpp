#include "lexer.h"
#include "rulereader.h"

#include <iterator>

bool RuleReader::read(const std::string& input) {
  Lexer.setInput(input);
  Lexer.scanTokens();
  Parser = LlamaParser(input, Lexer.tokens());
  std::vector<Rule> rules = Parser.parseRules(Lexer.ruleIndices());
  Rules.reserve(Rules.size() + rules.size());
  Rules.insert(
    Rules.end(),
    std::make_move_iterator(rules.begin()),
    std::make_move_iterator(rules.end())
  );
  return (Parser.errors().size() + Lexer.errors().size() == 0);
}
