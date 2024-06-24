#include <vector>
#include <string>
#include <regex>
#include <iostream>
#include <cctype>

/**************************************************************
rule_decl = RULE OPEN_BRACE META COLON expr+ CLOSE_BRACE
expr = IDENTIFIER EQUAL double_quoted_string
IDENTIFIER = "[a-zA-Z0-9-_]+"
double_quoted_string = "\""string"\""
string = "\w+"
NUMBER = [0-9]+
EQUAL = "="
RULE = "rule"
META = "meta"
HASH = "hash"
FILEMETADATA = "filemetadata"
SIGNATURE = "signature"
GREP = "grep"
OPEN_BRACE = "{"
COLON = ":"
CLOSE_BRACE = "}"
************************************************************/

enum class TokenType {
  NONE,

  RULE, META, FILEMETADATA, SIGNATURE, GREP, HASH,

  OPEN_BRACE, CLOSE_BRACE, COLON, EQUAL,
  IDENTIFIER, DOUBLE_QUOTED_STRING, NUMBER,

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
  Token(TokenType type, size_t start, size_t end) : Type(type), Start(start), End(end) {}

  TokenType Type;
  size_t Start, End;
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
  void parseNumber();

  void addToken(TokenType type, size_t start, size_t end) { Tokens.push_back(Token(type, start, end)); }

  char advance() {
    char curChar = getCurChar();
    ++CurIdx;
    return curChar;
  }

  bool isAtEnd() const { return CurIdx >= Input.size(); }

  char getCurChar() const;
  const std::vector<Token>& getTokens() const { return Tokens; }

private:
  const std::string& Input;
  size_t CurIdx;
  std::vector<Token> Tokens;
};