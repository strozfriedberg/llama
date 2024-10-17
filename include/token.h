#pragma once

#include <cstdint>
#include <unordered_map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>


enum class LlamaTokenType {
  NONE,

  // reserved keywords
  RULE,
  META,
  FILE_METADATA,
  SIGNATURE,
  GREP,
  PATTERNS,
  HASH,
  CONDITION,
  MD5,
  SHA1,
  SHA256,
  BLAKE3,
  ENCODINGS,
  NOCASE,
  FIXED,
  AND,
  OR,

  // punctuation
  OPEN_BRACE,
  CLOSE_BRACE,
  OPEN_PAREN,
  CLOSE_PAREN,
  COMMA,
  COLON,
  EQUAL,
  EQUAL_EQUAL,
  NOT_EQUAL,
  GREATER_THAN,
  GREATER_THAN_EQUAL,
  LESS_THAN,
  LESS_THAN_EQUAL,

  // user-defined tokens
  IDENTIFIER,
  DOUBLE_QUOTED_STRING,
  HEX_STRING,
  NUMBER,
  ENCODINGS_LIST,

  END_OF_FILE
};

const std::unordered_map<std::string_view, LlamaTokenType> LlamaKeywords = {
  {"rule", LlamaTokenType::RULE},
  {"meta", LlamaTokenType::META},
  {"file_metadata", LlamaTokenType::FILE_METADATA},
  {"signature", LlamaTokenType::SIGNATURE},
  {"grep", LlamaTokenType::GREP},
  {"patterns", LlamaTokenType::PATTERNS},
  {"hash", LlamaTokenType::HASH},
  {"condition", LlamaTokenType::CONDITION},
  {"md5", LlamaTokenType::MD5},
  {"sha1", LlamaTokenType::SHA1},
  {"sha256", LlamaTokenType::SHA256},
  {"blake3", LlamaTokenType::BLAKE3},
  {"encodings", LlamaTokenType::ENCODINGS},
  {"nocase", LlamaTokenType::NOCASE},
  {"fixed", LlamaTokenType::FIXED},
  {"and", LlamaTokenType::AND},
  {"or", LlamaTokenType::OR}
};

class LineCol {
public:
  std::string toString() const {
    std::string str("line ");
    str += std::to_string(LineNum);
    str += " column ";
    str += std::to_string(ColNum);
    return str;
  }

  uint32_t LineNum,
           ColNum;
};

class Token {
public:
  Token(LlamaTokenType type, std::string_view lexeme, LineCol pos)
       : Type(type), Lexeme(lexeme), Pos(pos){}

  size_t length() const { return Lexeme.length(); }

  LlamaTokenType Type;
  std::string_view Lexeme;
  LineCol Pos;
};

class UnexpectedInputError : public std::runtime_error {
public:
  UnexpectedInputError(const std::string_view& message, LineCol pos)
 : std::runtime_error(messageWithPos(message, pos)), Position(pos) {}

private:
  static std::string messageWithPos(std::string_view errMsg, LineCol pos) {
    std::string msg(errMsg);
    msg += " at ";
    msg += pos.toString();
    return msg;
  }

  LineCol Position;
};

