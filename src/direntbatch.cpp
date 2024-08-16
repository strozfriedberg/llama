#include "direntbatch.h"


void appendVal(duckdb_appender& appender, const char* s) {
  duckdb_state state = duckdb_append_varchar(appender, s);
  THROW_IF(state == DuckDBError, "Failed to append string value");
}

void appendVal(duckdb_appender& appender, uint64_t val) {
  duckdb_state state = duckdb_append_uint64(appender, val);
  THROW_IF(state == DuckDBError, "Failed to append uint64 value");
}

template<typename T>
void append(duckdb_appender& appender, T t) {
  appendVal(appender, t);
}

template<typename T, typename... Args>
void append(duckdb_appender& appender, T t, Args... args) {
  appendVal(appender, t);
  append(appender, args...);
}


void DirentBatch::copyToDB(duckdb_appender& appender) {
  duckdb_state state;
  for (uint32_t i = 0; i < Offsets.size(); ++i) {
    auto& offsets(Offsets[i]);
    auto& nums(Nums[i]);

    append(appender,
           Buf.data() + offsets.PathOffset,
           Buf.data() + offsets.NameOffset,
           Buf.data() + offsets.ShortOffset,
           Buf.data() + offsets.TypeOffset,
           Buf.data() + offsets.FlagsOffset,
           nums.MetaAddr,
           nums.ParentAddr,
           nums.MetaSeq,
           nums.ParentSeq
    );
    state = duckdb_appender_end_row(appender);
    THROW_IF(state == DuckDBError, "Failed call to end_row");
  }
  Buf.clear();
  Offsets.clear();
  Nums.clear();
}

