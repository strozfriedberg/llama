#include "lexer.h"
#include "rule_reader.h"

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
