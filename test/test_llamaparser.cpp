#include <catch2/catch_test_macros.hpp>
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
FILEMETADATA = "filemetadata"
SIGNATURE = "signature"
GREP = "grep"
LCB = "{"
COLON = ":"
RCB = "}"
************************************************************/

enum class TokenType {
  NONE,
  RULE,
  LCB,
  META,
  FILEMETADATA,
  SIGNATURE,
  GREP,
  COLON,
  RCB,
  ALPHA_NUM_UNDERSCORE,
  DOUBLE_QUOTED_STRING,
  STRING,
  EQUAL,
  ENDOFFILE
};

class Token {
public:
  Token(TokenType type, const std::string& lexeme = "") : lexeme(lexeme), type(type) {}

  std::string lexeme;
  TokenType type;
};

class UnexpectedInputError : public std::runtime_error {
public:
  UnexpectedInputError(const std::string& message) : std::runtime_error(message) {}
};

class LlamaLexer {
public:
  LlamaLexer(const std::string& input) : input(input), cur(input.data()) {};

  void scanTokens();
  void scanToken();

  void parseString();

  void addToken(TokenType type, const std::string& lexeme = "") { tokens.push_back(Token(type, lexeme)); }

  char advance() { return *cur++; }

  char peek() const { return *(cur + 1); }
  bool isAtEnd() const { return *cur == '\0'; }

  const std::vector<Token>& getTokens() const { return tokens; }

private:
  const std::string& input;
  const char* cur;
  std::vector<Token> tokens;
};

void print(std::string s) {
  std::cout << s << std::endl;
}

void LlamaLexer::scanTokens() {
  while (*cur != '\0') {
    scanToken();
  }

  tokens.push_back(Token(TokenType::ENDOFFILE));
}

void LlamaLexer::scanToken() {
  char c = advance();
  switch(c) {
    case '{': addToken(TokenType::LCB); break;
    case '}': addToken(TokenType::RCB); break;
    case ':': addToken(TokenType::COLON); break;
    case '=': addToken(TokenType::EQUAL); break;

    case '"': parseString(); break;

    case ' ':
    case '\n':
    case '\r':
    case '\t': break;

    default:
      throw UnexpectedInputError("Unexpected input character: " + std::string{c});
  }
}

void LlamaLexer::parseString() {
  std::string lexeme = std::string();
  while(*cur != '"' && !isAtEnd()) {
    lexeme.push_back(advance());
  }
  if (isAtEnd()) {
    throw UnexpectedInputError("Unterminated string");
  }
  advance(); // consume closing quote
  addToken(TokenType::DOUBLE_QUOTED_STRING, lexeme);
}

TEST_CASE("ScanToken") {
  std::string input = "{}:= \n\r\t";
  LlamaLexer lexer(input);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(0).type == TokenType::LCB);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(1).type == TokenType::RCB);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(2).type == TokenType::COLON);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(3).type == TokenType::EQUAL);
  lexer.scanToken();
  lexer.scanToken();
  lexer.scanToken();
  lexer.scanToken();
  REQUIRE(lexer.getTokens().size() == 4);
  REQUIRE(lexer.isAtEnd());
  REQUIRE_THROWS_AS(lexer.scanToken(), UnexpectedInputError);
}

TEST_CASE("ScanTokenString") {
  std::string input = "\"some string\"{";
  LlamaLexer lexer(input);
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(0).type == TokenType::DOUBLE_QUOTED_STRING);
  REQUIRE(lexer.getTokens().at(0).lexeme == "some string");
  lexer.scanToken();
  REQUIRE(lexer.getTokens().at(1).type == TokenType::LCB);
}

TEST_CASE("parseString") {
  std::string input = "some string\"";
  LlamaLexer lexer(input);
  lexer.parseString();
  REQUIRE(lexer.getTokens().at(0).type == TokenType::DOUBLE_QUOTED_STRING);
  REQUIRE(lexer.getTokens().at(0).lexeme == "some string");
}

TEST_CASE("unterminatedString") {
  std::string input = "some string";
  LlamaLexer lexer(input);
  REQUIRE_THROWS_AS(lexer.parseString(), UnexpectedInputError);
}

TEST_CASE("scanTokens") {
  std::string input = "{ }";
  LlamaLexer lexer(input);
  lexer.scanTokens();
  REQUIRE(lexer.getTokens().size() == 3);
  REQUIRE(lexer.getTokens()[0].type == TokenType::LCB);
  REQUIRE(lexer.getTokens()[1].type == TokenType::RCB);
  REQUIRE(lexer.getTokens()[2].type == TokenType::ENDOFFILE);
}
