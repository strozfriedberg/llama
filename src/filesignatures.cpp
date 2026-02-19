// file_signatures.cpp
//

#include <algorithm>
#include <cctype>
#include <exception>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <string_view>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#if defined(__clang__)
  #pragma GCC diagnostic ignored "-Wdeprecated-builtins"
#endif
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#pragma GCC diagnostic pop

#include "filesignatures.h"
#include "jsoncons_wrapper.h"
#include "util.h"

namespace fs = std::filesystem;

namespace FileSignatures {

LightGrep::LightGrep() : Prog(nullptr) {}

LightGrep::~LightGrep() {
  if (Prog) {
    lg_destroy_program(Prog);
  }
}

// return max_read
expected<size_t> LightGrep::setup(MagicsType const &m) {
  using namespace boost;

  size_t max_read = 0;
  try {
    LG_Error *err = 0;
    LG_HFSM fsm = lg_create_fsm(0, 0);
    destroy_guard fsm_guard([&fsm]() { lg_destroy_fsm(fsm); });

    for (std::size_t i = 0; i < m.size(); i++) {

      auto &p = m[i];

      if (p->Pattern.empty()) {
        continue;
      }

      LG_KeyOptions opt = {p->FixedString, p->CaseInsensitive, false};

      auto pattern = lg_create_pattern();
      destroy_guard pattern_guard(
          [&pattern]() { lg_destroy_pattern(pattern); });
      if (!lg_parse_pattern(pattern, p->Pattern.c_str(), &opt, &err)) {
        if (err) {
          auto r = makeUnexpected(err->Message);
          lg_free_error(err);
          return r;
        }
      }

      auto pattern_len = p->getPatternLength(false);
      if (pattern_len > max_read) {
        max_read = pattern_len;
      }

      for (auto const &encoding : p->Encodings) {
        if (!lg_add_pattern(fsm, pattern, encoding.c_str(), i, &err)) {
          if (err) {
            auto r = makeUnexpected(err->Message);
            lg_free_error(err);
            return r;
          }
        }
      }
    }

    LG_ProgramOptions opts = {0};
    if (!(Prog = lg_create_program(fsm, &opts))) {
      return makeUnexpected("lg_create_program() failed");
    }
  }
  catch (std::exception const &ex) {
    return makeUnexpected(ex.what());
  }

  return max_read;
}

expected<bool> LightGrep::search(const uint8_t *start, const uint8_t *end,
                                 void *user_data,
                                 LG_HITCALLBACK_FN callback_fn) const {
  try {
    LG_ContextOptions ctxOpts = {0, 0};
    LG_HCONTEXT searcher = lg_create_context(Prog, &ctxOpts);
    lg_reset_context(searcher);
    lg_starts_with(searcher, (const char *)start, (const char *)end, 0,
                   user_data, callback_fn);
    lg_destroy_context(searcher);
  }
  catch (std::exception const &ex) {
    return makeUnexpected(ex.what());
  }
  return true;
}

size_t getPatternLength(String const &pattern, bool only_significant) {
  std::size_t i = 0;
  size_t count = 0;
  char prev_c = 0;

  while (i < pattern.size()) {
    auto c = pattern[i];
    if (c == '\\') {
      if (pattern[i + 1] == 'x') {
        count += 1;
        i += 4;
      }
      else { // \\u0000
        count += 1;
        i += 6;
      }
    }
    else if (c == '{') {
      auto j = pattern.find('}', i + 1);
      auto i2 = pattern.find(',', i + 1);

      if (i2 < j) { i = i2; }

      auto x = std::stoi(pattern.substr(i + 1, j));
      i = j + 1;
      count += only_significant && prev_c == '.' ? 0 : x;
    }
    else if (c == '[') {
      auto j = pattern.find(']', i + 1);
      i = j + 1;
      count += 1;
    }
    else if (c == '.') {
      i += 1;
      count += only_significant ? 0 : 1;
    }
    else {
      count += 1;
      i += 1;
    }
    prev_c = c;
  }

  return count * 4;
}

size_t Magic::getPatternLength(bool only_significant) const {
  return FileSignatures::getPatternLength(Pattern, only_significant);
}

namespace {
void readPatterns(jsoncons::json const &magic_json, Magic &m) {
  if (magic_json.contains("pattern")) {
    m.Pattern = magic_json["pattern"].as_string();
  }
  m.FixedString = magic_json.contains("fixed_string")
                ? magic_json["fixed_string"].as_bool()
                : false;
  m.CaseInsensitive = magic_json.contains("case_insensitive")
                    ? magic_json["case_insensitive"].as_bool()
                    : false;
  if (magic_json.contains("encoding")) {
    boost::split(m.Encodings, magic_json["encoding"].as_string(), boost::is_any_of(","));
  }
  else {
    m.Encodings.push_back("ISO-8859-1");
  }
}

void readSpecs(jsoncons::json const &magic_json, Magic &m) {
  if (magic_json.contains("name")) {
    m.Name = magic_json["name"].as_string();
  }
  if (magic_json.contains("description")) {
    m.Description = magic_json["description"].as_string();
  }
  if (magic_json.contains("id")) {
    m.Id = magic_json["id"].as_string();
  }
  if (magic_json.contains("tags")) {
    for (const auto &tag : magic_json["tags"].array_range()) {
      m.Tags.push_back(tag.as_string());
    }
  }
  if (magic_json.contains("extensions")) {
    for (const auto &ext : magic_json["extensions"].object_range()) {
      m.Extensions[ext.key()] = ext.value().as_string();
    }
  }
}
} // namespace

expected<MagicsType> FileSigAnalyzer::readMagics(ReadSeek& rs) {
  try {
    // Read entire stream into a string
    rs.seek(0);
    size_t fileSize = rs.size();
    std::vector<uint8_t> buffer(fileSize);
    int64_t bytesRead = rs.read(fileSize, buffer.data());
    if (bytesRead < 0) {
      return makeUnexpected("Error reading from stream");
    }

    std::string jsonStr(buffer.begin(), buffer.begin() + bytesRead);
    auto json(jsoncons::json::parse(jsonStr));

    MagicsType magics;
    for (const auto &magic_json : json.array_range()) {
      Magic m;

      readPatterns(magic_json, m);
      readSpecs(magic_json, m);

      // Skip entries without patterns
      if (m.Pattern.empty()) {
        continue;
      }

      magics.push_back(std::make_shared<Magic>(m));
    }

    return magics;
  }
  catch (std::exception &e) {
    return makeUnexpected(e.what());
  }
}

struct lg_callback_context {
  const FileSigAnalyzer *self;
  std::vector<size_t>* hit_indices;
};

void FileSigAnalyzer::lgCallbackfn(void *userData,
                                   const LG_SearchHit *const hit) {
  auto ctx = (lg_callback_context *)userData;
  auto hit_info = lg_prog_pattern_info(ctx->self->Lg.get_lg_prog(), hit->KeywordIndex);
  if (hit_info) {
    ctx->hit_indices->push_back(hit_info->UserIndex);
  }
}

expected<bool> FileSigAnalyzer::lgSearch(const uint8_t *start,
                                         const uint8_t *end,
                                         std::vector<MagicPtr> &results) const {
  std::vector<size_t> hit_indices;
  lg_callback_context ctx{this, &hit_indices};

  auto lg_err = Lg.search(start, end, &ctx, &FileSigAnalyzer::lgCallbackfn);

  if (lg_err.has_error()) {
    return makeUnexpected("Lg.search() error: " + lg_err.error());
  }

  // Collect all matching signatures
  for (auto idx : hit_indices) {
    results.push_back(this->Magics[idx]);
  }

  return !results.empty();
}

FileSigAnalyzer::FileSigAnalyzer(const MagicsType& magics) {
  auto sorted_magics = magics;

  // resort magics by pattern size in descending order ('bigger' patterns first)
  std::sort(begin(sorted_magics), end(sorted_magics),
            [](MagicPtr const &a, MagicPtr const &b) -> bool {
              return a->getPatternLength(true) > b->getPatternLength(true);
            });

  this->Magics = std::move(sorted_magics);

  // Skip Lightgrep setup if there are no signatures
  if (this->Magics.empty()) {
    return;
  }

  auto r = Lg.setup(this->Magics);
  if (r.has_failure()) {
    throw std::runtime_error("LightGrep::setup failed: " + r.error());
  }

  auto max_read = r.value();
  ReadBuf.resize(max_read);
}

expected<bool> FileSigAnalyzer::getSignatures(ReadSeek& rs, std::vector<MagicPtr>& results) const {
  // If no signatures loaded, return early
  if (Magics.empty()) {
    return false;
  }

  // Seek to beginning of stream
  rs.seek(0);

  // Read up to ReadBuf.size() bytes
  int64_t bytes_read = rs.read(ReadBuf.size(), ReadBuf.data());
  if (bytes_read < 0) {
    return makeUnexpected("Failed to read from stream");
  }

  if (bytes_read == 0) {
    // Empty file - no signatures to detect
    return false;
  }

  // Search for signatures
  return lgSearch(ReadBuf.data(), ReadBuf.data() + bytes_read, results);
}

} // namespace FileSignatures
