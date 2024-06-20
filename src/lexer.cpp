#include "lexer.h"

void print(std::string s) {
  std::cout << s << std::endl;
}

void LlamaLexer::scanTokens() {
  while (!isAtEnd()) {
    scanToken();
  }

  Tokens.push_back(Token(TokenType::ENDOFFILE));
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

void LlamaLexer::parseIdentifier() {
  std::string lexeme;
  while (isalnum(*Cur) || *Cur == '_') {
    lexeme.push_back(advance());
  }

  auto found = Llama::keywords.find(lexeme);

  if (found != Llama::keywords.end()) {
    addToken(found->second);
  }
  else {
    addToken(TokenType::ALPHA_NUM_UNDERSCORE, lexeme);
  }
}

void LlamaLexer::parseString() {
  std::string lexeme = std::string();
  while(*Cur != '"' && !isAtEnd()) {
    lexeme.push_back(advance());
  }
  if (isAtEnd()) {
    throw UnexpectedInputError("Unterminated string");
  }
  advance(); // consume closing quote
  addToken(TokenType::DOUBLE_QUOTED_STRING, lexeme);
}