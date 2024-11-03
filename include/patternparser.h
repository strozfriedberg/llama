#include "lightgrep/api.h"

#include "parser.h"

class PatternParser {
public:
  PatternParser() : Pat(lg_create_pattern()), Err(nullptr) {}

  ~PatternParser() {
    lg_destroy_pattern(Pat);
    lg_free_error(Err);
  }

  void parse(PatternDef pDef) {
    lg_parse_pattern(Pat, pDef.Pattern.c_str(), &pDef.Options, &Err);
  }
private:
  LG_HPATTERN Pat;
  LG_Error* Err;
};