#include <vector>
#include <string>
#include <regex>
#include <iostream>
#include <cctype>

/**************************************************************
rule_decl = RULE LCB META COLON expr+ RCB
expr = alpha_num_underscore EQUAL double_quoted_string
alpha_num_underscore = "[a-zA-Z0-9-_]+"
double_quoted_string = "\""string"\""
string = "\w+"
EQUAL = "="
RULE = "rule"
META = "meta"
HASH = "hash"
FILEMETADATA = "filemetadata"
SIGNATURE = "signature"
GREP = "grep"
LCB = "{"
COLON = ":"
RCB = "}"
************************************************************/

enum class TokenType {
  NONE,

  RULE, META, FILEMETADATA, SIGNATURE, GREP, HASH,

  LCB, RCB, COLON, EQUAL,
  ALPHA_NUM_UNDERSCORE, DOUBLE_QUOTED_STRING,

  ENDOFFILE
};


namespace Llama {
  const std::unordered_map<std::string, TokenType> keywords = {
    {"rule", TokenType::RULE},
    {"meta", TokenType::META},
    {"filemetadata", TokenType::FILEMETADATA},
    {"signature", TokenType::SIGNATURE},
    {"grep", TokenType::GREP},
    {"hash", TokenType::HASH}
  };
}

class Token {
public:
  Token(TokenType type, const std::string& lexeme = "") : Lexeme(lexeme), Type(type) {}

  // replace with start, end
  std::string Lexeme;
  TokenType Type;
};

class UnexpectedInputError : public std::runtime_error {
public:
  UnexpectedInputError(const std::string& message) : std::runtime_error(message) {}
};

class LlamaLexer {
public:
  LlamaLexer(const std::string& input) : Input(input), Cur(Input.begin()) {};

  void scanTokens();
  void scanToken();

  void parseIdentifier();
  void parseString();

  void addToken(TokenType type, const std::string& lexeme = "") { Tokens.push_back(Token(type, lexeme)); }

  char advance() { return *Cur++; }

  char peek() const { return *(Cur + 1); }
  bool isAtEnd() const { return Cur == Input.end(); }

  const std::vector<Token>& getTokens() const { return Tokens; }

private:
  const std::string& Input;
  std::string::const_iterator Cur;
  std::vector<Token> Tokens;
};