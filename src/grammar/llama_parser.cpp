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

#include "llama_parser.h"
#include <iostream>

namespace LlamaRegex {
  std::regex alphanum("\\w");
}

void print(std::string s) {
  std::cout << s << std::endl;
}

LlamaLexer::LlamaLexer(char* input) {
  curr = input;
  lastToken = new Token();

  while (*curr != '\0') {
    std::string lexeme = getNextLexeme();
    Token* token = new Token(lexeme);
    lastToken = token;
    tokens.push_back(token);
  }
}

std::string LlamaLexer::getNextLexeme() {
  std::string lexeme = "";
  if (*curr == ' ' || *curr == '\n') {
    // skip whitespace
    curr++;
  }
  if (*curr == '"') {
    lexeme.push_back(*(curr++)); // capture opening quote
    while (*curr != '"') {
      lexeme += *curr;
      curr++;
    }
    lexeme.push_back(*(curr++)); // capture closing quote
  }
  else if (std::regex_match(curr, curr + 1, LlamaRegex::alphanum)) {
    while (std::regex_match(curr, curr + 1, LlamaRegex::alphanum)) {
      lexeme += *curr;
      curr++;
    }
  }
  else {
    while (*curr != ' ' && *curr != '\0') {
      lexeme += *curr;
      curr++;
    }
  }
  print(lexeme);
  return lexeme;
}

Token::Token() {
  lexeme = "";
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
  } else if (is_colon(lexeme)) {
    type = TokenType::COLON;
  } else if (is_alpha_num_underscore(lexeme)) {
    type = TokenType::ALPHA_NUM_UNDERSCORE;
  } else if (is_equal(lexeme)) {
    type = TokenType::EQUAL;
  } else if (is_double_quoted_string(lexeme)) {
    type = TokenType::DOUBLE_QUOTED_STRING;
    // strip quotes
    lexeme = lexeme.substr(1, lexeme.size() - 2);
  } else if (is_rcb(lexeme)) {
    type = TokenType::RCB;
  }
}


bool Token::is_rule(std::string c) { return c == "rule"; }
bool Token::is_lcb(std::string c) { return c == "{"; }
bool Token::is_meta(std::string c) { return c == "meta"; }
bool Token::is_colon(std::string c) { return c == ":"; }
bool Token::is_alpha_num_underscore(std::string c) { std::regex e("\\w+"); return std::regex_match(c.begin(), c.end(), e); }
bool Token::is_equal(std::string c) { return c == "="; }
bool Token::is_double_quoted_string(std::string c) { return (c[0] == '"' && c[c.size() - 1] == '"'); }
bool Token::is_rcb(std::string c) { return c == "}";}