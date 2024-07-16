#pragma once

#include <unordered_map>
#include <tuple>

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
  CREATED,
  MODIFIED,
  FILESIZE,
  FILENAME,
  FILEPATH,
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
    {"strings", TokenType::STRINGS},
    {"hash", TokenType::HASH},
    {"condition", TokenType::CONDITION},
    {"created", TokenType::CREATED},
    {"modified", TokenType::MODIFIED},
    {"filesize", TokenType::FILESIZE},
    {"filename", TokenType::FILENAME},
    {"filepath", TokenType::FILEPATH},
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
    {"fixed", TokenType::FIXED}
  };
}

class LineCol {
public:
  uint32_t LineNum,
           ColNum;
};

class Token {
public:
  Token(TokenType type, uint32_t start, uint32_t end, LineCol pos)
       : Type(type), Start(start), End(end), Pos(pos){}

  TokenType Type;
  uint32_t Start, End;
  LineCol Pos;
};
