#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <boost/outcome.hpp>
#include <lightgrep/api.h>

namespace FileSignatures {

template <typename T>
using expected = boost::outcome_v2::result<T, std::string>;

inline auto makeUnexpected(const std::string &s) {
  return boost::outcome_v2::failure(s);
}

enum class CompareType { Eq, EqUpper, Ne, Gt, Lt, And, Xor, Or, Nor };

struct OffsetType {
  long count;
  bool from_start;
};

using Binary = std::vector<uint8_t>;

struct Magic {
  struct Check {
    CompareType CompareOp;
    OffsetType Offset;
    Binary Value;
    Binary PreProcess;

    bool compare(Binary const &data) const;
  };
  std::vector<Check> Checks;
  std::string Description;
  std::string Id;
  std::unordered_map<std::string, std::string> Extensions;
  std::string Pattern;
  bool FixedString;
  bool CaseInsensetive;
  std::vector<std::string> Encodings;
  std::vector<std::string> Tags;

  size_t getPatternLength(bool only_significant = false) const;
};

size_t getPatternLength(std::string const &pattern, bool only_significant);

typedef std::shared_ptr<Magic> MagicPtr;

typedef std::vector<MagicPtr> MagicsType;

class LightGrep {
  LG_HPROGRAM Prog;

public:
  LightGrep();
  ~LightGrep();
  expected<size_t> setup(MagicsType const &m);
  expected<bool> search(const uint8_t *start, const uint8_t *end,
                        void *user_data, LG_HITCALLBACK_FN callback_fn) const;
  LG_HPROGRAM get_lg_prog() const { return Prog; }
};

class FileSigAnalyzer {
  MagicsType Magics;
  std::unordered_map<std::string, MagicPtr> SignatureDict;
  MagicsType SignatureList;

  LightGrep Lg;
  // size of the buffer - max value of getPatternLength(false)
  Binary ReadBuf;

  static void lgCallbackfn(void *userData, const LG_SearchHit *const hit);

public:
  FileSigAnalyzer();
  static expected<MagicsType> readMagics(std::string_view path);
  expected<bool> getSignature(const std::filesystem::directory_entry &de,
                              MagicPtr &result) const;
};

inline bool startsWith(const std::string &s, const std::string &prefix) {
  return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

OffsetType parseOffset(std::string s);
uint8_t char2uint8(char input);
// accept 0xABCD (or 1234), return [0xAB, 0xCD] (or [12, 34])
Binary str2bin(const std::string &src);

} // namespace FileSignatures