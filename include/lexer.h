#include <cctype>
#include <cstdint>
#include <iostream>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>


enum class TokenType {
  NONE,

  // reserved keywords
  RULE,
  META,
  FILE_METADATA,
  SIGNATURE,
  GREP,
  STRINGS,
  HASH,
  CONDITION,
  CREATED_TIME,
  MODIFIED_TIME,
  FILESIZE,
  FILENAME,
  FILEPATH,
  ALL,
  ANY,
  OFFSET,
  COUNT,
  COUNT_HAS_HITS,
  LENGTH,


  // punctuation
  OPEN_BRACE,
  CLOSE_BRACE,
  COLON,
  EQUAL,

  // user-defined tokens
  IDENTIFIER,
  DOUBLE_QUOTED_STRING,
  NUMBER,

  END_OF_FILE
};


namespace Llama {
  const std::unordered_map<std::string, TokenType> keywords = {
    {"rule", TokenType::RULE},
    {"meta", TokenType::META},
    {"file_metadata", TokenType::FILE_METADATA},
    {"signature", TokenType::SIGNATURE},
    {"grep", TokenType::GREP},
    {"strings", TokenType::STRINGS},
    {"hash", TokenType::HASH},
    {"condition", TokenType::CONDITION},
    {"created_time", TokenType::CREATED_TIME},
    {"modified_time", TokenType::MODIFIED_TIME},
    {"filesize", TokenType::FILESIZE},
    {"filename", TokenType::FILENAME},
    {"filepath", TokenType::FILEPATH},
    {"all", TokenType::ALL},
    {"any", TokenType::ANY},
    {"offset", TokenType::OFFSET},
    {"count", TokenType::COUNT},
    {"count_has_hits", TokenType::COUNT_HAS_HITS},
    {"length", TokenType::LENGTH}
  };
}

class Token {
public:
  Token(TokenType type, uint32_t start, uint32_t end) : Type(type), Start(start), End(end) {}

  TokenType Type;
  uint32_t Start, End;
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

  void addToken(TokenType type, uint32_t start, uint32_t end) { Tokens.push_back(Token(type, start, end)); }

  char advance() {
    char curChar = getCurChar();
    ++CurIdx;
    return curChar;
  }

  bool match(char expected);
  bool isAtEnd() const { return CurIdx >= Input.size(); }

  char getCurChar() const;
  const std::vector<Token>& getTokens() const { return Tokens; }

private:
  const std::string& Input;
  size_t CurIdx;
  std::vector<Token> Tokens;
};