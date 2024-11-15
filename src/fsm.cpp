#include <iostream>
#include <fsm.h>
#include <parser.h>

void LgFsmHolder::addPatterns(const PatternPair& pair, const LlamaParser& parser) {
  uint64_t patternIndex = 0;
  if (pair.second.Enc.first == pair.second.Enc.second) {
    // No encodings were defined for the pattern, so parse with ASCII only
    addPattern(PatParser.parse(pair.second), "ASCII", patternIndex);
    ++patternIndex;
  }
  else {
    for (uint64_t i = pair.second.Enc.first; i < pair.second.Enc.second; i += 2) {
      std::cout << std::string(parser.Tokens[i].Lexeme).c_str() << "\n";
      addPattern(PatParser.parse(pair.second), std::string(parser.Tokens[i].Lexeme).c_str(), patternIndex);
      ++patternIndex;
    }
  }
}