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
  Token(TokenType type) : lexeme(std::string()), type(type) {}
  Token(TokenType type, std::string lexeme) : lexeme(lexeme), type(type) {}

  std::string lexeme;
  TokenType type;
};

class UnexpectedInputError : public std::runtime_error {
  std::string message;
public:
  UnexpectedInputError(std::string message) : std::runtime_error(message) {}
};

class LlamaLexer {
public:
  LlamaLexer(char* input);
  std::vector<Token> getTokens() { return tokens; }
  void scanTokens();

  char advance() { return *curr++; }
  char peek() const { return *(curr + 1); }

  void scanToken();

  void addToken(TokenType type, const std::string& lexeme) { tokens.push_back(Token(type, lexeme)); }
  void addToken(TokenType type) { addToken(type, std::string()); }

  bool isAtEnd() const { return *curr == '\0'; }


  void parseString();

  char* curr;
  std::vector<Token> tokens;
};

void print(std::string s) {
  std::cout << s << std::endl;
}

LlamaLexer::LlamaLexer(char* input) {
  curr = input;
}

void LlamaLexer::scanTokens() {
  while (*curr != '\0') {
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
  while(*curr != '"' && !isAtEnd()) {
    lexeme.push_back(advance());
  }
  if (isAtEnd()) {
    throw UnexpectedInputError("Unterminated string");
  }
  advance(); // consume closing quote
  addToken(TokenType::DOUBLE_QUOTED_STRING, lexeme);
}

TEST_CASE("ScanToken") {
  char* input = "{}:= \n\r\t";
  LlamaLexer lexer(input);
  lexer.scanToken();
  REQUIRE(lexer.tokens.at(0).type == TokenType::LCB);
  lexer.scanToken();
  REQUIRE(lexer.tokens.at(1).type == TokenType::RCB);
  lexer.scanToken();
  REQUIRE(lexer.tokens.at(2).type == TokenType::COLON);
  lexer.scanToken();
  REQUIRE(lexer.tokens.at(3).type == TokenType::EQUAL);
  lexer.scanToken();
  lexer.scanToken();
  lexer.scanToken();
  lexer.scanToken();
  REQUIRE(lexer.tokens.size() == 4);
  REQUIRE(lexer.isAtEnd());
  REQUIRE_THROWS_AS(lexer.scanToken(), UnexpectedInputError);
}

TEST_CASE("ScanTokenString") {
  char* input = "\"some string\"{";
  LlamaLexer lexer(input);
  lexer.scanToken();
  REQUIRE(lexer.tokens.at(0).type == TokenType::DOUBLE_QUOTED_STRING);
  REQUIRE(lexer.tokens.at(0).lexeme == "some string");
  lexer.scanToken();
  REQUIRE(lexer.tokens.at(1).type == TokenType::LCB);
}

TEST_CASE("parseString") {
  char* input = "some string\"";
  LlamaLexer lexer(input);
  lexer.parseString();
  REQUIRE(lexer.tokens.at(0).type == TokenType::DOUBLE_QUOTED_STRING);
  REQUIRE(lexer.tokens.at(0).lexeme == "some string");
}

TEST_CASE("unterminatedString") {
  char* input = "some string";
  LlamaLexer lexer(input);
  REQUIRE_THROWS_AS(lexer.parseString(), UnexpectedInputError);
}

TEST_CASE("scanTokens") {
  char* input = "{ }";
  LlamaLexer lexer(input);
  lexer.scanTokens();
  REQUIRE(lexer.getTokens().size() == 3);
  REQUIRE(lexer.getTokens()[0].type == TokenType::LCB);
  REQUIRE(lexer.getTokens()[1].type == TokenType::RCB);
  REQUIRE(lexer.getTokens()[2].type == TokenType::ENDOFFILE);
}
