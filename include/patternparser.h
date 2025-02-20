#pragma once

#include "lightgrep/api.h"

#include "parser.h"

class PatternParser {
public:
  PatternParser() : Pat(lg_create_pattern()), Err(nullptr) {}

  ~PatternParser() {
    lg_destroy_pattern(Pat);
    lg_free_error(Err);
  }

  LG_HPATTERN parse(PatternDef pDef) {
    if (lg_parse_pattern(Pat, pDef.Pattern.c_str(), &pDef.Options, &Err) == 0) {
      throw std::runtime_error("Lightgrep failed to parse pattern " + pDef.Pattern + "\n" + Err->Message);
    };
    return Pat;
  }

  LG_HPATTERN Pat;
  LG_Error* Err;
};