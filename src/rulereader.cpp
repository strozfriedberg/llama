#include "lexer.h"
#include "rulereader.h"

int RuleReader::read(const std::string& input) {
  LlamaLexer lexer(input);
  try {
    lexer.scanTokens();
    Parser = LlamaParser(input, lexer.getTokens());
    std::vector<Rule> rules = Parser.parseRules(lexer.getRuleIndices().size());
    Rules.reserve(Rules.size() + rules.size());
    Rules.insert(Rules.end(), rules.begin(), rules.end());
  }
  catch (UnexpectedInputError& e) {
    LastError = e.what();
    return -1;
  }

  return Rules.size();
}
