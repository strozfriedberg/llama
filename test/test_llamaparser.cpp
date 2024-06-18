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
  EQUAL
};

class Token {
public:
  Token();
  Token(std::string lexeme);
  Token(TokenType type) : lexeme(std::string()), type(type) {}
  Token(std::string lexeme, TokenType type) : lexeme(lexeme), type(type) {}
  TokenType getType() { return type; }

private:
  bool is_rule(std::string c);
  bool is_lcb(std::string c);
  bool is_meta(std::string c);
  bool is_filemetadata(std::string c);
  bool is_signature(std::string c);
  bool is_grep(std::string c);
  bool is_colon(std::string c);
  bool is_alpha_num_underscore(std::string c);
  bool is_equal(std::string c);
  bool is_double_quoted_string(std::string c);
  bool is_rcb(std::string c);

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
  std::vector<Token*> getTokens() { return tokens; }

private:
  std::string getNextLexeme();
  char advance() { return *curr++; }
  void scanToken();
  void addToken(TokenType type, std::string lexeme) { tokens.push_back(new Token(lexeme, type)); }
  void addToken(TokenType type) { addToken(type, std::string()); }

  char* curr;
  std::vector<Token*> tokens;
};

void print(std::string s) {
  std::cout << s << std::endl;
}

LlamaLexer::LlamaLexer(char* input) {
  curr = input;

  while (*curr != '\0') {
    std::string lexeme = getNextLexeme();
    Token* token = new Token(lexeme);
    tokens.push_back(token);
  }
}

void LlamaLexer::scanToken() {
  char c = advance();
  switch(c) {
    case '{': addToken(TokenType::LCB); break;
    case '}': addToken(TokenType::RCB); break;
    case ':': addToken(TokenType::COLON); break;
    case '=': addToken(TokenType::EQUAL); break;

    case ' ':
    case '\n':
    case '\r':
    case '\t': break;

    default:
      throw UnexpectedInputError("Unexpected input character: " + std::string{c});
  }
}

std::string LlamaLexer::getNextLexeme() {
  std::string lexeme = "";

  // ignore whitespace
  while (std::isspace(*curr)) {
    advance();
  }

  // handle double quoted strings
  // this is a different branch because we must handle
  // white space within double quoted strings
  if (*curr == '"') {
    // capture opening quote
    lexeme.push_back(advance());

    while (*curr != '"') {
      lexeme += *curr;
      advance();
    }

    // capture closing quote
    lexeme.push_back(advance());
  }
  // process punctuation one character at a time
  else if (std::ispunct(*curr)) {
    lexeme += *curr;
    advance();
  }
  // handle identifiers which must start with an alphanumeric character
  else if (std::isalnum(*curr)) {
    while (std::isalnum(*curr) || *curr == '_' || *curr == '-') {
      lexeme += *curr;
      advance();
    }
  }
  else {
    while (!std::isspace(*curr) && *curr != '\0') {
      lexeme += *curr;
      advance();
    }
  }
  print(lexeme);
  return lexeme;
}

Token::Token() {
  this->lexeme = "";
  type = TokenType::NONE;
}

Token::Token(std::string lexeme) {
  this->lexeme = lexeme;
  if (is_rule(lexeme)) {
    type = TokenType::RULE;
  } else if (is_lcb(lexeme)) {
    type = TokenType::LCB;
  } else if (is_meta(lexeme)) {
    type = TokenType::META;
  } else if (is_filemetadata(lexeme)) {
    type = TokenType::FILEMETADATA;
  } else if (is_signature(lexeme)) {
    type = TokenType::SIGNATURE;
  } else if (is_grep(lexeme)) {
    type = TokenType::GREP;
  } else if (is_colon(lexeme)) {
    type = TokenType::COLON;
  } else if (is_alpha_num_underscore(lexeme)) {
    type = TokenType::ALPHA_NUM_UNDERSCORE;
  } else if (is_equal(lexeme)) {
    type = TokenType::EQUAL;
  } else if (is_double_quoted_string(lexeme)) {
    type = TokenType::DOUBLE_QUOTED_STRING;
    // strip quotes
    this->lexeme = lexeme.substr(1, lexeme.size() - 2);
  } else if (is_rcb(lexeme)) {
    type = TokenType::RCB;
  }
}


bool Token::is_rule(std::string c) {
  return c == "rule";
}

bool Token::is_lcb(std::string c) {
  return c == "{";
}

bool Token::is_meta(std::string c) {
  return c == "meta";
}

bool Token::is_filemetadata(std::string c) {
  return c == "filemetadata";
}

bool Token::is_signature(std::string c) {
  return c == "signature";
}

bool Token::is_grep(std::string c) {
  return c == "grep";
}

bool Token::is_colon(std::string c) {
  return c == ":";
}

bool Token::is_alpha_num_underscore(std::string c) {
  std::regex e("\\w+");
  return std::regex_match(c.begin(), c.end(), e);
}

bool Token::is_equal(std::string c) {
  return c == "=";
}

bool Token::is_double_quoted_string(std::string c) {
  return (c[0] == '"' && c[c.size() - 1] == '"');
}

bool Token::is_rcb(std::string c) {
  return c == "}";
}



TEST_CASE("RuleWithMetaSectionAndOneAssignment") {
  char* input = "rule { meta: some_id = \"some_value\" }";
  LlamaLexer lexer(input);
  REQUIRE(lexer.getTokens().size() == 8);
  REQUIRE(lexer.getTokens()[0]->getType() == TokenType::RULE);
  REQUIRE(lexer.getTokens()[1]->getType() == TokenType::LCB);
  REQUIRE(lexer.getTokens()[2]->getType() == TokenType::META);
  REQUIRE(lexer.getTokens()[3]->getType() == TokenType::COLON);
  REQUIRE(lexer.getTokens()[4]->getType() == TokenType::ALPHA_NUM_UNDERSCORE);
  REQUIRE(lexer.getTokens()[5]->getType() == TokenType::EQUAL);
  REQUIRE(lexer.getTokens()[6]->getType() == TokenType::DOUBLE_QUOTED_STRING);
  REQUIRE(lexer.getTokens()[7]->getType() == TokenType::RCB);
}

TEST_CASE("RuleWithMultipleSpaces") {
  char* input = "rule    {   meta:    some_id  = \"some_value\"   }";
  LlamaLexer lexer(input);
  REQUIRE(lexer.getTokens().size() == 8);
  REQUIRE(lexer.getTokens()[0]->getType() == TokenType::RULE);
  REQUIRE(lexer.getTokens()[1]->getType() == TokenType::LCB);
  REQUIRE(lexer.getTokens()[2]->getType() == TokenType::META);
  REQUIRE(lexer.getTokens()[3]->getType() == TokenType::COLON);
  REQUIRE(lexer.getTokens()[4]->getType() == TokenType::ALPHA_NUM_UNDERSCORE);
  REQUIRE(lexer.getTokens()[5]->getType() == TokenType::EQUAL);
  REQUIRE(lexer.getTokens()[6]->getType() == TokenType::DOUBLE_QUOTED_STRING);
  REQUIRE(lexer.getTokens()[7]->getType() == TokenType::RCB);
}

TEST_CASE("RuleWithNewLinesAndTabs") {
  char* input = "rule {\n\tmeta:\n}";
  LlamaLexer lexer(input);
  REQUIRE(lexer.getTokens().size() == 5);
  REQUIRE(lexer.getTokens()[0]->getType() == TokenType::RULE);
  REQUIRE(lexer.getTokens()[1]->getType() == TokenType::LCB);
  REQUIRE(lexer.getTokens()[2]->getType() == TokenType::META);
  REQUIRE(lexer.getTokens()[3]->getType() == TokenType::COLON);
  REQUIRE(lexer.getTokens()[4]->getType() == TokenType::RCB);
}

TEST_CASE("RuleWithMultipleAssignments") {
  char* input = "rule { meta: some_id = \"some_value\"\n\t\tsome_other_id = \"some_other_value\" }";
  LlamaLexer lexer(input);
  REQUIRE(lexer.getTokens().size() == 11);
  REQUIRE(lexer.getTokens()[0]->getType() == TokenType::RULE);
  REQUIRE(lexer.getTokens()[1]->getType() == TokenType::LCB);
  REQUIRE(lexer.getTokens()[2]->getType() == TokenType::META);
  REQUIRE(lexer.getTokens()[3]->getType() == TokenType::COLON);
  REQUIRE(lexer.getTokens()[4]->getType() == TokenType::ALPHA_NUM_UNDERSCORE);
  REQUIRE(lexer.getTokens()[5]->getType() == TokenType::EQUAL);
  REQUIRE(lexer.getTokens()[6]->getType() == TokenType::DOUBLE_QUOTED_STRING);
  REQUIRE(lexer.getTokens()[7]->getType() == TokenType::ALPHA_NUM_UNDERSCORE);
  REQUIRE(lexer.getTokens()[8]->getType() == TokenType::EQUAL);
  REQUIRE(lexer.getTokens()[9]->getType() == TokenType::DOUBLE_QUOTED_STRING);
  REQUIRE(lexer.getTokens()[10]->getType() == TokenType::RCB);
}

TEST_CASE("RuleWithMetaFilemetadataGrepSignatureSections") {
  char* input = "rule { meta: some_id = \"some_value\" filemetadata: grep: signature: }";
  LlamaLexer lexer(input);
  REQUIRE(lexer.getTokens().size() == 14);
  REQUIRE(lexer.getTokens()[0]->getType() == TokenType::RULE);
  REQUIRE(lexer.getTokens()[1]->getType() == TokenType::LCB);
  REQUIRE(lexer.getTokens()[2]->getType() == TokenType::META);
  REQUIRE(lexer.getTokens()[3]->getType() == TokenType::COLON);
  REQUIRE(lexer.getTokens()[4]->getType() == TokenType::ALPHA_NUM_UNDERSCORE);
  REQUIRE(lexer.getTokens()[5]->getType() == TokenType::EQUAL);
  REQUIRE(lexer.getTokens()[6]->getType() == TokenType::DOUBLE_QUOTED_STRING);
  REQUIRE(lexer.getTokens()[7]->getType() == TokenType::FILEMETADATA);
  REQUIRE(lexer.getTokens()[8]->getType() == TokenType::COLON);
  REQUIRE(lexer.getTokens()[9]->getType() == TokenType::GREP);
  REQUIRE(lexer.getTokens()[10]->getType() == TokenType::COLON);
  REQUIRE(lexer.getTokens()[11]->getType() == TokenType::SIGNATURE);
  REQUIRE(lexer.getTokens()[12]->getType() == TokenType::COLON);
  REQUIRE(lexer.getTokens()[13]->getType() == TokenType::RCB);
}