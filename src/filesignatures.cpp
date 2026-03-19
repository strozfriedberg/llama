// ABOUTME: File signature detection using Lightgrep pattern matching
// ABOUTME: Compiles magic byte patterns and searches file headers for matches

#include <fstream>
#include <iostream>
#include <string>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#if defined(__clang__)
  #pragma GCC diagnostic ignored "-Wdeprecated-builtins"
#endif
#include <boost/algorithm/string.hpp>
#pragma GCC diagnostic pop

#include "filesignatures.h"
#include "jsoncons_wrapper.h"
#include "util.h"

namespace fs = std::filesystem;

Lightgrep::Lightgrep(std::shared_ptr<::ProgramHandle> prog)
  : Prog(std::move(prog)), Ctx(nullptr)
{
  if (Prog) {
    createContext();
  }
}

Lightgrep::~Lightgrep() {
  if (Ctx) {
    lg_destroy_context(Ctx);
  }
}

Lightgrep::Lightgrep(Lightgrep&& other) noexcept
  : Prog(std::move(other.Prog)), Ctx(other.Ctx)
{
  other.Ctx = nullptr;
}

Lightgrep& Lightgrep::operator=(Lightgrep&& other) noexcept {
  if (this != &other) {
    if (Ctx) {
      lg_destroy_context(Ctx);
    }
    Prog = std::move(other.Prog);
    Ctx = other.Ctx;
    other.Ctx = nullptr;
  }
  return *this;
}

void Lightgrep::createContext() {
  LG_ContextOptions ctxOpts = {0, 0};
  Ctx = lg_create_context(Prog.get(), &ctxOpts);
  if (!Ctx) {
    throw std::runtime_error("lg_create_context() failed");
  }
}

std::shared_ptr<::ProgramHandle> Lightgrep::compile(const MagicsType& m) {
  LG_Error *err = nullptr;
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
        std::string msg(err->Message);
        lg_free_error(err);
        throw std::runtime_error("lg_parse_pattern failed: " + msg);
      }
    }

    for (auto const &encoding : p->Encodings) {
      if (!lg_add_pattern(fsm, pattern, encoding.c_str(), i, &err)) {
        if (err) {
          std::string msg(err->Message);
          lg_free_error(err);
          throw std::runtime_error("lg_add_pattern failed: " + msg);
        }
      }
    }
  }

  LG_ProgramOptions opts = {0};
  LG_HPROGRAM rawProg = lg_create_program(fsm, &opts);
  if (!rawProg) {
    throw std::runtime_error("lg_create_program() failed");
  }
  return std::shared_ptr<ProgramHandle>(rawProg, lg_destroy_program);
}

void Lightgrep::search(const uint8_t *start, const uint8_t *end,
                                 void *user_data,
                                 LG_HITCALLBACK_FN callback_fn) {
  lg_reset_context(Ctx);
  lg_starts_with(Ctx, (const char *)start, (const char *)end, 0,
                 user_data, callback_fn);
}

void Lightgrep::writeProgram(const std::shared_ptr<::ProgramHandle>& prog, const std::string& path) {
  unsigned int size = lg_program_size(prog.get());
  std::vector<char> buffer(size);
  lg_write_program(prog.get(), buffer.data());

  std::ofstream out(path, std::ios::binary);
  if (!out) {
    throw std::runtime_error("Failed to open file for writing: " + path);
  }
  out.write(buffer.data(), size);
  if (!out) {
    throw std::runtime_error("Failed to write program to: " + path);
  }
}

std::shared_ptr<::ProgramHandle> Lightgrep::readProgram(const std::string& path) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in) {
    throw std::runtime_error("Failed to open file for reading: " + path);
  }

  auto size = in.tellg();
  in.seekg(0);
  std::vector<char> buffer(size);
  in.read(buffer.data(), size);
  if (!in) {
    throw std::runtime_error("Failed to read program from: " + path);
  }

  LG_HPROGRAM rawProg = lg_read_program(buffer.data(), static_cast<int>(size));
  if (!rawProg) {
    throw std::runtime_error("lg_read_program() failed");
  }
  return std::shared_ptr<ProgramHandle>(rawProg, lg_destroy_program);
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

MagicsType FileSigAnalyzer::readMagics(ReadSeek& rs) {
  rs.seek(0);
  size_t fileSize = rs.size();
  std::vector<uint8_t> buffer(fileSize);
  int64_t bytesRead = rs.read(fileSize, buffer.data());
  if (bytesRead < 0) {
    throw std::runtime_error("Error reading from stream");
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

void FileSigAnalyzer::lgSearch(const uint8_t *start,
                               const uint8_t *end,
                               std::vector<MagicPtr> &results) {
  std::vector<size_t> hit_indices;
  lg_callback_context ctx{this, &hit_indices};

  try {
    Lg.search(start, end, &ctx, &FileSigAnalyzer::lgCallbackfn);
  }
  catch (const std::exception& e) {
    std::cerr << "Signature search error: " << e.what() << std::endl;
    return;
  }

  for (auto idx : hit_indices) {
    results.push_back(this->Magics[idx]);
  }
}

FileSigAnalyzer::FileSigAnalyzer(std::shared_ptr<::ProgramHandle> prog, const MagicsType& magics)
  : Magics(magics), Lg(std::move(prog)), ReadBuf(4096)
{
}

bool FileSigAnalyzer::getSignatures(ReadSeek& rs, std::vector<MagicPtr>& results) {
  if (Magics.empty()) {
    return false;
  }

  rs.seek(0);

  int64_t bytes_read = rs.read(ReadBuf.size(), ReadBuf.data());
  if (bytes_read < 0) {
    throw std::runtime_error("Failed to read from stream for signature detection");
  }

  if (bytes_read == 0) {
    return false;
  }

  lgSearch(ReadBuf.data(), ReadBuf.data() + bytes_read, results);
  return !results.empty();
}
