#pragma once

#include <cstdint>
#include <filesystem>

#include "jsoncons_wrapper.h"
#include "filesignatures.h"

namespace fs = std::filesystem;

namespace DirUtils {
  std::string fileTypeString(fs::file_type type);

  std::uintmax_t fileSize(const fs::directory_entry& de);
  fs::file_type fileType(const fs::directory_entry& de);
  std::uintmax_t nlink(const fs::directory_entry& de);
  fs::file_time_type mtime(const fs::directory_entry& de);
}

class DirConverter {
  Magics magics;
  std::map<std::string, std::shared_ptr<magic>> signature_dict;
  Magics signature_list;

  LightGrep lg;
  // size of the buffer - max value of get_pattern_length(false)
  Binary read_buf;

  static void lg_callbackfn(void* userData, const LG_SearchHit* const hit);
  void get_signature(const fs::directory_entry& de, std::string* sig_desc, std::vector<std::string>* sig_tags) const;

public:
  DirConverter();
  jsoncons::json convertMeta(const fs::directory_entry& de) const;
  jsoncons::json convertName(const fs::directory_entry& de) const;
  jsoncons::json convertAttrs(const fs::directory_entry& de) const;
  jsoncons::json convertAttr(const fs::directory_entry& de) const;

private:

};
