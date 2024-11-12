#include "lexer.h"
#include "rulereader.h"

bool RuleReader::read(const std::string& input) {
  Lexer.setInput(input);
  Lexer.scanTokens();
  Parser = LlamaParser(input, Lexer.getTokens());
  std::vector<Rule> rules = Parser.parseRules(Lexer.getRuleIndices());
  Rules.reserve(Rules.size() + rules.size());
  Rules.insert(Rules.end(), rules.begin(), rules.end());
  return (Parser.getErrors().size() + Lexer.getErrors().size() == 0);
}
