#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <boost/outcome.hpp>
#include <boost/range/iterator_range.hpp>
#include <lightgrep/api.h>

namespace FileSignatures {

template <typename T>
using expected = boost::outcome_v2::result<T, std::string>;

using Binary = std::vector<uint8_t>;

inline auto makeUnexpected(const std::string &s) {
  return boost::outcome_v2::failure(s);
}

enum class CompareType { Eq, EqUpper, Ne, Gt, Lt, And, Xor, Or, Nor };

struct Offset {
  long count;
  bool from_start;
};

struct magic {
  struct check {
    CompareType compare_type;
    Offset offset;
    Binary value;
    Binary pre_process;

    bool compare(Binary const &data) const;
  };
  std::vector<check> checks;
  std::string description;
  std::string id;
  std::map<std::string, std::string> extensions;
  std::string pattern;
  bool fixed_string;
  bool case_insensetive;
  std::vector<std::string> encodings;
  std::vector<std::string> tags;

  size_t get_pattern_length(bool only_significant = false) const;
};

typedef std::shared_ptr<magic> Magic;

size_t get_pattern_length(std::string const &pattern, bool only_significant);

typedef std::vector<Magic> Magics;

typedef boost::iterator_range<const uint8_t *> MemoryRegion;

class LightGrep {
  LG_HPROGRAM _prog;

public:
  LightGrep();
  ~LightGrep();
  expected<size_t> setup(Magics const &m);
  expected<bool> search(MemoryRegion const &region, void *user_data,
                        LG_HITCALLBACK_FN callback_fn) const;
  LG_HPROGRAM get_lg_prog() const { return _prog; }
};

class FileSigAnalyzer {
  Magics magics;
  std::map<std::string, Magic> signature_dict;
  Magics signature_list;

  LightGrep lg;
  // size of the buffer - max value of get_pattern_length(false)
  Binary read_buf;

  static void lg_callbackfn(void *userData, const LG_SearchHit *const hit);

public:
  FileSigAnalyzer();
  static expected<Magics> readMagics(std::string_view path);
  expected<bool> get_signature(const std::filesystem::directory_entry &de,
                               Magic &result) const;
};

inline bool startsWith(const std::string &s, const std::string &prefix) {
  return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

Offset parseOffset(std::string s);
uint8_t char2uint8(char input);
// accept 0xABCD (or 1234), return [0xAB, 0xCD] (or [12, 34])
Binary str2bin(const std::string &src);

} // namespace FileSignatures