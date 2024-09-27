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
  CREATED,
  MODIFIED,
  FILESIZE,
  FILENAME,
  FILEPATH,
  ID,
  NAME,
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

const std::unordered_map<std::string, LlamaTokenType> LlamaKeywords = {
  {"rule", LlamaTokenType::RULE},
  {"meta", LlamaTokenType::META},
  {"file_metadata", LlamaTokenType::FILE_METADATA},
  {"signature", LlamaTokenType::SIGNATURE},
  {"grep", LlamaTokenType::GREP},
  {"patterns", LlamaTokenType::PATTERNS},
  {"hash", LlamaTokenType::HASH},
  {"condition", LlamaTokenType::CONDITION},
  {"created", LlamaTokenType::CREATED},
  {"modified", LlamaTokenType::MODIFIED},
  {"filesize", LlamaTokenType::FILESIZE},
  {"filename", LlamaTokenType::FILENAME},
  {"filepath", LlamaTokenType::FILEPATH},
  {"id", LlamaTokenType::ID},
  {"name", LlamaTokenType::NAME},
  {"all", LlamaTokenType::ALL},
  {"any", LlamaTokenType::ANY},
  {"offset", LlamaTokenType::OFFSET},
  {"count", LlamaTokenType::COUNT},
  {"count_has_hits", LlamaTokenType::COUNT_HAS_HITS},
  {"length", LlamaTokenType::LENGTH},
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
  Token(LlamaTokenType type, uint64_t start, uint64_t end, LineCol pos)
       : Type(type), Start(start), End(end), Pos(pos){}

  size_t length() const { return End - Start; }

  LlamaTokenType Type;
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

