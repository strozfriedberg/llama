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

#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>

#include "filesignatures.h"
#include "jsoncons_wrapper.h"
#include "util.h"

namespace fs = std::filesystem;

namespace FileSignatures {

LightGrep::LightGrep() {}

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

      LG_KeyOptions opt = {p->FixedString, p->CaseInsensetive, false};

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
  } catch (std::exception const &ex) {
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
  } catch (std::exception const &ex) {
    return makeUnexpected(ex.what());
  }
  return true;
}

expected<CompareType> parse_compare_type(std::string_view s) {
  static const std::map<std::string_view, CompareType> dict = {
      {"=", CompareType::Eq},   {"=/c", CompareType::EqUpper},
      {"!", CompareType::Ne},   {">", CompareType::Gt},
      {"<", CompareType::Lt},   {"&", CompareType::And},
      {"^", CompareType::Xor},  {"OR", CompareType::Or},
      {"NOR", CompareType::Nor}};

  if (auto val = dict.find(s); val != dict.end()) {
    return val->second;
  }

  return makeUnexpected(std::string("Unknown compareType: ") + std::string(s));
}

bool iequals(char lhs, char rhs) {
  std::locale loc;
  return std::toupper(lhs, loc) == std::toupper(rhs, loc);
}

bool Magic::Check::compare(Binary const &data) const {
  if (data.size() < Value.size())
    return false;
  auto eq = [](uint8_t const &a, uint8_t const &b) -> bool { return a == b; };
  auto eqUp = [](uint8_t const &a, uint8_t const &b) -> bool {
    return iequals((char)a, (char)b);
  };
  auto ne = [](uint8_t const &a, uint8_t const &b) -> bool { return a != b; };
  auto gt = [](uint8_t const &a, uint8_t const &b) -> bool { return a > b; };
  auto lt = [](uint8_t const &a, uint8_t const &b) -> bool { return a < b; };
  auto _and = [](uint8_t const &a, uint8_t const &b) -> bool { return a == b; };
  auto _xor = [](uint8_t const &a, uint8_t const &b) -> bool {
    return ~((int8_t)a ^ (int8_t)b);
  };
  auto _or = [](uint8_t const &a, uint8_t const &b) -> bool { return a | b; };
  auto nor = [](uint8_t const &a, uint8_t const &b) -> bool {
    return !(a | b);
  };
  std::function<bool(uint8_t const &, uint8_t const &)> fn;
  switch (CompareOp) {
  case CompareType::Eq:
    fn = eq;
    break;
  case CompareType::EqUpper:
    fn = eqUp;
    break;
  case CompareType::Ne:
    fn = ne;
    break;
  case CompareType::Gt:
    fn = gt;
    break;
  case CompareType::Lt:
    fn = lt;
    break;
  case CompareType::And:
    fn = _and;
    break;
  case CompareType::Xor:
    fn = _xor;
    break;
  case CompareType::Or:
    fn = _or;
    break;
  case CompareType::Nor:
    fn = nor;
    break;
  default:
    throw std::range_error("compare: impossible OP " +
                           std::to_string((int)CompareOp));
  }

  bool result = true;
  bool need_pp = (PreProcess.size() > 0);
  for (auto data_value = data.cbegin(), expected_value = Value.cbegin();
       result && expected_value != Value.cend();
       data_value++, expected_value++) {
    auto v = (need_pp) ? *data_value &
                             *(PreProcess.begin() +
                               (data_value - data.cbegin()) % PreProcess.size())
                       : *data_value;
    result = fn(v, *expected_value);
  }
  return result;
}

size_t getPatternLength(std::string const &pattern, bool only_significant) {
  std::size_t i = 0;
  size_t count = 0;
  char prev_c = 0;

  while (i < pattern.size()) {
    auto c = pattern[i];
    if (c == '\\') {
      if (pattern[i + 1] == 'x') {
        count += 1;
        i += 4;
      } else { // \\u0000
        count += 1;
        i += 6;
      }
    } else if (c == '{') {
      auto j = pattern.find('}', i + 1);
      auto i2 = pattern.find(',', i + 1);

      if (i2 < j)
        i = i2;

      auto x = std::stoi(pattern.substr(i + 1, j));
      i = j + 1;
      count += only_significant && prev_c == '.' ? 0 : x;
    } else if (c == '[') {
      auto j = pattern.find(']', i + 1);
      i = j + 1;
      count += 1;
    } else if (c == '.') {
      i += 1;
      count += only_significant ? 0 : 1;
    } else {
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

OffsetType parseOffset(std::string s) {
  std::stringstream ss;
  bool from_start = true;

  if (startsWith(s, "Z")) {
    s = s.substr(1);
    from_start = false;
  }
  if (startsWith(s, "0x") || startsWith(s, "0X")) {
    s = s.substr(2);
    ss << std::hex << s;
  } else {
    ss << std::dec << s;
  }
  long v;
  ss >> v;

  if (!from_start)
    v *= -1;

  return OffsetType{v, from_start};
}

uint8_t char2uint8(char input) {
  if (input >= '0' && input <= '9')
    return input - '0';
  if (input >= 'A' && input <= 'F')
    return input - 'A' + 10;
  if (input >= 'a' && input <= 'f')
    return input - 'a' + 10;
  return 0;
}

// accept 0xABCD (or 1234), return [0xAB, 0xCD] (or [12, 34])
Binary str2bin(const std::string &src) {
  bool hex = startsWith(src, "0x") || startsWith(src, "0X");
  Binary dst(src.length() / 2 - hex);
  auto di = dst.begin();
  for (size_t i = hex * 2; i < src.length(); i += 2) {
    *di++ = (char2uint8(src[i]) * (hex ? 16 : 10) + char2uint8(src[i + 1]));
  }
  return dst;
}

expected<MagicsType> FileSigAnalyzer::readMagics(std::string_view path) {
  try {
    std::ifstream is(path.data());
    if (is.fail())
      return makeUnexpected(std::string("Error: bad path ") +
                            std::string(path));

    auto json(jsoncons::json::parse(is));

    MagicsType magics;
    for (const auto &magic_json : json.array_range()) {
      Magic m;
      if (magic_json.contains("checks")) {
        for (const auto &check : magic_json["checks"].array_range()) {
          if (auto compare_type =
                  parse_compare_type(check["compare_type"].as_string())) {
            auto preprocess = check.contains("pre_process")
                                  ? str2bin(check["pre_process"].as_string())
                                  : Binary();

            m.Checks.push_back(Magic::Check{
                compare_type.value(), parseOffset(check["offset"].as_string()),
                str2bin(check["value"].as_string()), preprocess});
          } else {
            return makeUnexpected(compare_type.error());
          }
        }
      }
      if (magic_json.contains("pattern")) {
        m.Pattern = magic_json["pattern"].as_string();
      }
      m.FixedString = magic_json.contains("fixed_string")
                          ? magic_json["fixed_string"].as_bool()
                          : false;
      m.CaseInsensetive = magic_json.contains("case_insensitive")
                              ? magic_json["case_insensitive"].as_bool()
                              : false;

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
      if (magic_json.contains("encoding")) {
        boost::split(m.Encodings, magic_json["encoding"].as_string(),
                     boost::is_any_of(","));
      } else {
        m.Encodings.push_back("ISO-8859-1");
      }
      magics.push_back(std::make_shared<Magic>(m));
    }

    return magics;
  } catch (std::exception &e) {
    return makeUnexpected(e.what());
  }
}

struct lg_callback_context {
  const FileSigAnalyzer *self;
  size_t min_hit_index;
};

void FileSigAnalyzer::lgCallbackfn(void *userData,
                                   const LG_SearchHit *const hit) {
  auto ctx = (lg_callback_context *)userData;
  auto hit_info =
      lg_prog_pattern_info(ctx->self->Lg.get_lg_prog(), hit->KeywordIndex);
  if (hit_info && hit_info->UserIndex < ctx->min_hit_index) {
    ctx->min_hit_index = hit_info->UserIndex;
  }
}

expected<bool> FileSigAnalyzer::getSignature(const fs::directory_entry &de,
                                             MagicPtr &result) const {
  std::error_code ec;
  if (!de.is_regular_file(ec) || ec) {
    return makeUnexpected("FileSigAnalyzer is working with regular files only");
  }
  std::ifstream ifs(de.path(), std::ios::binary);
  if (ifs) {
    auto ext = de.path().extension().u8string();
    if (!ext.empty()) {
      boost::algorithm::to_upper(ext);
      if (ext.length() > 1) {
        // clean dot
        ext = ext.substr(1);
      }
    }

    lg_callback_context ctx{this, std::numeric_limits<size_t>::max()};
    auto readed = ifs.read((char *)ReadBuf.data(), ReadBuf.size()).gcount();
    if (readed == 0) {
      return makeUnexpected("read zero bytes from " + de.path().string());
    }
    auto lg_err = Lg.search(ReadBuf.data(), ReadBuf.data() + readed, &ctx,
                            &FileSigAnalyzer::lgCallbackfn);
    if (lg_err.has_error()) {
      throw std::runtime_error(
          "Lg.search() failed on file: " + de.path().string() +
          std::string(", error: ") + lg_err.error());
    }
    if (ctx.min_hit_index != std::numeric_limits<size_t>::max()) {
      // hit
      result = this->Magics[ctx.min_hit_index];
      return true;
    }

    // no hits? search manually

    Binary check_buf(ReadBuf);

    auto get_buf = [&ifs, this, &check_buf](OffsetType const &offset,
                                            std::size_t size) {
      if (size + offset.count > ReadBuf.size() || offset.from_start == false) {
        check_buf.resize(size);
        ifs.clear();
        ifs.seekg(offset.count,
                  offset.from_start ? std::ios_base::beg : std::ios_base::end);
        auto readed =
            ifs.read((char *)check_buf.data(), check_buf.size()).gcount();
        if (readed != (std::streamsize)size)
          throw std::runtime_error(
              ("read(" + std::to_string(size) + ") at " +
                   std::to_string(offset.count) + ", ",
               std::to_string(offset.from_start) + " failed."));
        return check_buf;
      }
      return Binary(&ReadBuf[offset.count], &ReadBuf[offset.count + size]);
    };

    // by "ext"
    auto s = SignatureDict.find(ext);
    if (s != SignatureDict.end()) {
      auto &m = s->second;
      bool all_checks_passed = m->Checks.size() > 0;
      BOOST_FOREACH (auto check_it, m->Checks) {
        if (!(all_checks_passed =
                  (check_it.compare(get_buf(check_it.Offset,
                                            check_it.Value.size())) == true)))
          break;
      }
      if (all_checks_passed) {
        // hit
        result = m;
        return true;
      }
    }

    // final check through all signatures
    for (auto const &s : SignatureList) {
      bool all_checks_passed = false;
      BOOST_FOREACH (auto check_it, s->Checks) {
        if (!(all_checks_passed =
                  (check_it.compare(get_buf(check_it.Offset,
                                            check_it.Value.size())) == true)))
          break;
      }
      if (all_checks_passed) {
        result = s;
        return true;
      }
    }
  }
  return false;
}

FileSigAnalyzer::FileSigAnalyzer() {
  std::string magics_file("./magics.json");
  auto result = readMagics(magics_file);
  if (result.has_error()) {
    throw std::runtime_error("Couldn't open file: " + magics_file +
                             std::string(", ") + result.error());
  }

  auto magics = result.value();

  // fill SignatureDict & SignatureList
  for (auto const &m : magics) {
    SignatureList.push_back(m);
    for (auto const &ext : m->Extensions) {
      if (SignatureDict.count(ext.first) == 0) {
        SignatureDict.insert(std::pair(ext.first, m));
      }
    }
  }

  // resort magics by pattern size in desceding order ('bigger' patterns first)
  std::sort(begin(magics), end(magics),
            [](MagicPtr const &a, MagicPtr const &b) -> bool {
              return a->getPatternLength(true) > b->getPatternLength(true);
            });

  this->Magics = std::move(magics);

  auto r = Lg.setup(this->Magics);
  if (r.has_failure()) {
    throw std::runtime_error("LightGrep::setup failed: " + r.error());
  }

  auto max_read = r.value();
  ReadBuf.resize(max_read);
}

} // namespace FileSignatures