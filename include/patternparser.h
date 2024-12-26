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
    lg_parse_pattern(Pat, pDef.Pattern.c_str(), &pDef.Options, &Err);
    return Pat;
  }

  LG_HPATTERN Pat;
  LG_Error* Err;
};