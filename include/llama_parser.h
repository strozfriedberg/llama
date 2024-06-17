/**************************************************************
rule_decl = RULE LCB META COLON expr+ RCB
expr = alpha_num_underscore EQUAL double_quoted_string
alpha_num_underscore = "[a-zA-Z0-9-_]+"
double_quoted_string = "\""string"\""
string = "\w+"
EQUAL = "="
RULE = "rule"
LCB = "{"
META = "meta"
COLON = ":"
RCB = "}"
************************************************************/

#include <vector>
#include <string>
#include <regex>

enum class TokenType {
  NONE,
  RULE,
  LCB,
  META,
  COLON,
  RCB,
  ALPHA_NUM_UNDERSCORE,
  DOUBLE_QUOTED_STRING,
  STRING,
  EQUAL
};

class Token {
public:
  Token();
  Token(std::string lexeme);
  TokenType getType() { return type; }

private:
  bool is_rule(std::string c);
  bool is_lcb(std::string c);
  bool is_meta(std::string c);
  bool is_colon(std::string c);
  bool is_alpha_num_underscore(std::string c);
  bool is_equal(std::string c);
  bool is_double_quoted_string(std::string c);
  bool is_rcb(std::string c);

  std::string lexeme;
  TokenType type;
};

class LlamaLexer {
public:
  LlamaLexer(char* input);
  std::vector<Token*> getTokens() { return tokens; }

private:
  std::string getNextLexeme();

  char* curr;
  Token* lastToken;
  std::vector<Token*> tokens;
};
