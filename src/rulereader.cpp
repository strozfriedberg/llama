#include "lexer.h"
#include "rulereader.h"

bool RuleReader::read(const std::string& input, const std::string& source) {
  Lexer.setInput(input);
  Lexer.scanTokens(source);
  Parser = LlamaParser(input, Lexer.tokens());
  std::vector<Rule> rules = Parser.parseRules(Lexer.ruleIndices(), source);
  Rules.reserve(Rules.size() + rules.size());
  Rules.insert(Rules.end(), rules.begin(), rules.end());
  return (Parser.errors().size() + Lexer.errors().size() == 0);
}
