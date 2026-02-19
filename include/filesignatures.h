#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <boost/outcome.hpp>
#include <lightgrep/api.h>

#include "readseek.h"

namespace FileSignatures {

template <typename T>
using expected = boost::outcome_v2::result<T, std::string>;

inline auto makeUnexpected(const std::string &s) {
  return boost::outcome_v2::failure(s);
}

inline auto makeOk() { return boost::outcome_v2::success(); }

using Binary = std::vector<uint8_t>;
using String = std::string;
using Strings = std::vector<String>;
using String2StringMap = std::unordered_map<String, String>;

struct Magic {
  String Name;
  String Description;
  String Id;
  String2StringMap Extensions;
  String Pattern;
  bool FixedString;
  bool CaseInsensitive;
  Strings Encodings;
  Strings Tags;

  size_t getPatternLength(bool only_significant = false) const;
};

using MagicPtr = std::shared_ptr<Magic>;
using MagicsType = std::vector<MagicPtr>;
using String2MagicMap = std::unordered_map<String, MagicPtr>;

size_t getPatternLength(String const &pattern, bool only_significant);

class LightGrep {
  LG_HPROGRAM Prog;

public:
  LightGrep();
  ~LightGrep();

  expected<size_t> setup(MagicsType const &m);
  expected<bool> search(const uint8_t *start, const uint8_t *end, void *user_data, LG_HITCALLBACK_FN callback_fn) const;
  LG_HPROGRAM get_lg_prog() const { return Prog; }
};

class FileSigAnalyzer {
  MagicsType Magics;
  LightGrep Lg;
  mutable std::vector<uint8_t> ReadBuf;

  // Each instance has its own LG context for thread safety
  // (LG program is compiled once and shared, but search context is per-instance)

  expected<bool> lgSearch(const uint8_t* start, const uint8_t* end,
                          std::vector<MagicPtr>& results) const;
  static void lgCallbackfn(void* userData, const LG_SearchHit* const hit);

public:
  // Takes pre-parsed signatures. Compiles Lightgrep program and allocates read buffer.
  FileSigAnalyzer(const MagicsType& magics);

  static expected<MagicsType> readMagics(ReadSeek& rs);

  // Detect signatures in a ReadSeek stream. Populates results with all matches.
  expected<bool> getSignatures(ReadSeek& rs, std::vector<MagicPtr>& results) const;
};

inline bool startsWith(const std::string &s, const std::string &prefix) {
  return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

} // namespace FileSignatures