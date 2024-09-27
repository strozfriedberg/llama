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

enum class LlamaLiteral {
  ST_RULE = (int)(LlamaTokenType::RULE),
  ST_META = (int)(LlamaTokenType::META),
  ST_FILE_METADATA = (int)(LlamaTokenType::FILE_METADATA),
  ST_SIGNATURE = (int)(LlamaTokenType::SIGNATURE),
  ST_GREP = (int)(LlamaTokenType::GREP),
  ST_PATTERNS = (int)(LlamaTokenType::PATTERNS),
  ST_HASH = (int)(LlamaTokenType::HASH),
  ST_CONDITION = (int)(LlamaTokenType::CONDITION),
  ST_CREATED = (int)(LlamaTokenType::CREATED),
  ST_MODIFIED = (int)(LlamaTokenType::MODIFIED),
  ST_FILESIZE = (int)(LlamaTokenType::FILESIZE),
  ST_FILENAME = (int)(LlamaTokenType::FILENAME),
  ST_FILEPATH = (int)(LlamaTokenType::FILEPATH),
  ST_ID = (int)(LlamaTokenType::ID),
  ST_NAME = (int)(LlamaTokenType::NAME),
  ST_ALL = (int)(LlamaTokenType::ALL),
  ST_ANY = (int)(LlamaTokenType::ANY),
  ST_OFFSET = (int)(LlamaTokenType::OFFSET),
  ST_COUNT = (int)(LlamaTokenType::COUNT),
  ST_COUNT_HAS_HITS = (int)(LlamaTokenType::COUNT_HAS_HITS),
  ST_LENGTH = (int)(LlamaTokenType::LENGTH),
  ST_MD5 = (int)(LlamaTokenType::MD5),
  ST_SHA1 = (int)(LlamaTokenType::SHA1),
  ST_SHA256 = (int)(LlamaTokenType::SHA256),
  ST_BLAKE3 = (int)(LlamaTokenType::BLAKE3),
  ST_ENCODINGS = (int)(LlamaTokenType::ENCODINGS),
  ST_NOCASE = (int)(LlamaTokenType::NOCASE),
  ST_FIXED = (int)(LlamaTokenType::FIXED),
  ST_AND = (int)(LlamaTokenType::AND),
  ST_OR = (int)(LlamaTokenType::OR),
  ST_ASCII
};

static const std::vector<std::string> Literals = {
  "rule",
  "meta",
  "file_metadata",
  "signature",
  "grep",
  "patterns",
  "hash",
  "condition",
  "created",
  "modified",
  "filesize",
  "filename",
  "filepath",
  "id",
  "name",
  "all",
  "any",
  "offset",
  "count",
  "count_has_hits",
  "length",
  "md5",
  "sha1",
  "sha256",
  "blake3",
  "encodings",
  "nocase",
  "fixed",
  "and",
  "or",
  "ASCII"
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

