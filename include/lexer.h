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
  Token(TokenType type, int32_t start, int32_t end) : Type(type), Start(start), End(end) {}

  int32_t Start, End;
  TokenType Type;
};

class UnexpectedInputError : public std::runtime_error {
public:
  UnexpectedInputError(const std::string& message) : std::runtime_error(message) {}
};

class LlamaLexer {
public:
  LlamaLexer(const std::string& input) : Input(input), CurIdx(0) {};

  void scanTokens();
  void scanToken();

  void parseIdentifier();
  void parseString();

  void addToken(TokenType type, int32_t start, int32_t end) { Tokens.push_back(Token(type, start, end)); }

  char advance() { return Input.at(CurIdx++); }

  bool isAtEnd() const { return CurIdx >= Input.size(); }

  char getCurChar() const;
  const std::vector<Token>& getTokens() const { return Tokens; }

private:
  const std::string& Input;
  int32_t CurIdx;
  std::vector<Token> Tokens;
};