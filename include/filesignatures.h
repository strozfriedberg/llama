#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <lightgrep/api.h>

#include "readseek.h"

struct Magic {
  std::string Name;
  std::string Description;
  std::string Id;
  std::unordered_map<std::string, std::string> Extensions;
  std::string Pattern;
  bool FixedString;
  bool CaseInsensitive;
  std::vector<std::string> Encodings;
  std::vector<std::string> Tags;

};

using MagicPtr = std::shared_ptr<Magic>;
using MagicsType = std::vector<MagicPtr>;

class Lightgrep {
  std::shared_ptr<::ProgramHandle> Prog;
  LG_HCONTEXT Ctx;

  void createContext();

public:
  explicit Lightgrep(std::shared_ptr<::ProgramHandle> prog = nullptr);
  ~Lightgrep();

  Lightgrep(const Lightgrep&) = delete;
  Lightgrep& operator=(const Lightgrep&) = delete;
  Lightgrep(Lightgrep&& other) noexcept;
  Lightgrep& operator=(Lightgrep&& other) noexcept;

  void search(const uint8_t *start, const uint8_t *end, void *user_data, LG_HITCALLBACK_FN callback_fn);

  LG_HPROGRAM get_lg_prog() const { return Prog.get(); }

  static std::shared_ptr<::ProgramHandle> compile(const MagicsType& magics);
  static void writeProgram(const std::shared_ptr<::ProgramHandle>& prog, const std::string& path);
  static std::shared_ptr<::ProgramHandle> readProgram(const std::string& path);
};

class FileSigAnalyzer {
  MagicsType Magics;
  Lightgrep Lg;
  mutable std::vector<uint8_t> ReadBuf;

  void lgSearch(const uint8_t* start, const uint8_t* end,
                std::vector<MagicPtr>& results);
  static void lgCallbackfn(void* userData, const LG_SearchHit* const hit);

public:
  FileSigAnalyzer(std::shared_ptr<::ProgramHandle> prog, const MagicsType& magics);

  static MagicsType readMagics(ReadSeek& rs);

  // Detect signatures in a ReadSeek stream. Populates results with all matches.
  bool getSignatures(ReadSeek& rs, std::vector<MagicPtr>& results);
};

inline bool startsWith(const std::string &s, const std::string &prefix) {
  return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}
