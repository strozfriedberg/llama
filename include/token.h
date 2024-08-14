#pragma once

#include <cstdint>
#include <unordered_map>
#include <stdexcept>
#include <string>
#include <tuple>


enum class TokenType {
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
  CREATED,
  MODIFIED,
  FILESIZE,
  FILENAME,
  FILEPATH,
  ID,
  EXTENSION,
  ALL,
  ANY,
  OFFSET,
  COUNT,
  COUNT_HAS_HITS,
  LENGTH,
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

namespace Llama {
  const std::unordered_map<std::string, TokenType> keywords = {
    {"rule", TokenType::RULE},
    {"meta", TokenType::META},
    {"file_metadata", TokenType::FILE_METADATA},
    {"signature", TokenType::SIGNATURE},
    {"grep", TokenType::GREP},
    {"patterns", TokenType::PATTERNS},
    {"hash", TokenType::HASH},
    {"condition", TokenType::CONDITION},
    {"created", TokenType::CREATED},
    {"modified", TokenType::MODIFIED},
    {"filesize", TokenType::FILESIZE},
    {"filename", TokenType::FILENAME},
    {"filepath", TokenType::FILEPATH},
    {"id", TokenType::ID},
    {"extension", TokenType::EXTENSION},
    {"all", TokenType::ALL},
    {"any", TokenType::ANY},
    {"offset", TokenType::OFFSET},
    {"count", TokenType::COUNT},
    {"count_has_hits", TokenType::COUNT_HAS_HITS},
    {"length", TokenType::LENGTH},
    {"md5", TokenType::MD5},
    {"sha1", TokenType::SHA1},
    {"sha256", TokenType::SHA256},
    {"blake3", TokenType::BLAKE3},
    {"encodings", TokenType::ENCODINGS},
    {"nocase", TokenType::NOCASE},
    {"fixed", TokenType::FIXED},
    {"and", TokenType::AND},
    {"or", TokenType::OR}
  };
}

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
  Token(TokenType type, uint64_t start, uint64_t end, LineCol pos)
       : Type(type), Start(start), End(end), Pos(pos){}

  size_t length() const { return End - Start; }

  TokenType Type;
  uint64_t Start, End;
  LineCol Pos;
};

class UnexpectedInputError : public std::runtime_error {
public:
  UnexpectedInputError(const std::string& message, LineCol pos)
  : std::runtime_error(message), Position(pos) {}

  std::string messageWithPos() const {
    std::string msg(what());
    msg += " at ";
    msg += Position.toString();
    return msg;
  }

  LineCol Position;
};
