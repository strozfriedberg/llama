#pragma once

#include <cstdint>
#include <filesystem>

#include "jsoncons_wrapper.h"

namespace fs = std::filesystem;

namespace DirUtils {
  std::string fileTypeString(fs::file_type type);

  std::uintmax_t fileSize(const fs::directory_entry& de);
  fs::file_type fileType(const fs::directory_entry& de);
  std::uintmax_t nlink(const fs::directory_entry& de);
  fs::file_time_type mtime(const fs::directory_entry& de);
}

struct magic {
  struct check {
    std::string compare_type;
    unsigned long long offset;
    std::vector<char> value;
  };
  std::vector<check> checks;
  std::string description;
  std::map<std::string, std::string> extensions;
  std::string pattern;
  std::vector<std::string> tags;
};

class DirConverter {
  std::vector<magic> magics;

  void readMagics();
public:
  DirConverter();
  jsoncons::json convertMeta(const fs::directory_entry& de) const;
  jsoncons::json convertName(const fs::directory_entry& de) const;
  jsoncons::json convertAttrs(const fs::directory_entry& de) const;
  jsoncons::json convertAttr(const fs::directory_entry& de) const;

private:

};
