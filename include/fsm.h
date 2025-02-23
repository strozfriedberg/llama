#pragma once

#include <iostream>
#include <string>
#include <string_view>

#include "lightgrep/api.h"
#include "patternparser.h"

class LlamaParser;
struct PatternDef;

using PatternPair = std::pair<std::string_view, PatternDef>;

class LgFsmHolder
{
public:
  LgFsmHolder() : Fsm(lg_create_fsm(0, 0)), Err(nullptr) {}
  ~LgFsmHolder() { lg_destroy_fsm(Fsm); lg_free_error(Err); }

  void addPattern(LG_HPATTERN pat, std::string_view enc, uint64_t patIdx) {
    lg_add_pattern(Fsm, pat, std::string(enc).c_str(), patIdx, &Err);
  }

  void addPatterns(const PatternPair& pair, const LlamaParser& parser, const std::string& ruleId, std::vector<std::string>& patToRuleId);

  LG_HFSM getFsm() const { return Fsm; }
  LG_Error* Error() const { return Err; }

private:
  PatternParser PatParser;
  LG_HFSM Fsm;
  LG_Error* Err;
};