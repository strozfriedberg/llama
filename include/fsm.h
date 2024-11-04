#include <string>
#include <string_view>

#include "lightgrep/api.h"

class LlamaLgFsm {
public:
  LlamaLgFsm() : Fsm(lg_create_fsm(0,0)), Err(nullptr) {}
  ~LlamaLgFsm() { lg_destroy_fsm(Fsm); lg_free_error(Err); }

  void addPattern(LG_HPATTERN pat, std::string_view enc, uint64_t patIdx) {
    lg_add_pattern(Fsm, pat, std::string(enc).c_str(), patIdx, &Err);
  }

  LG_HFSM getFsm() const { return Fsm; }

private:
  LG_HFSM Fsm;
  LG_Error* Err;
};