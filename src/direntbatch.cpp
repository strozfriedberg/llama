#include "direntbatch.h"


void append_val(duckdb_appender& appender, const char* s) {
  duckdb_append_varchar(appender, s);
}

void append_val(duckdb_appender& appender, uint64_t val) {
  duckdb_append_uint64(appender, val);
}

template<typename T>
void append(duckdb_appender& appender, T t) {
  append_val(appender, t);
}

template<typename T, typename... Args>
void append(duckdb_appender& appender, T t, Args... args) {
  append_val(appender, t);
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
/*      state = duckdb_append_varchar(appender, Buf.data() + offsets.PathOffset);
      THROW_IF(state == DuckDBError, "Failed to append path");
      state = duckdb_append_varchar(appender, Buf.data() + offsets.NameOffset);
      THROW_IF(state == DuckDBError, "Failed to append name");
      state = duckdb_append_varchar(appender, Buf.data() + offsets.ShortOffset);
      THROW_IF(state == DuckDBError, "Failed to append shrt_name");
      state = duckdb_append_varchar(appender, Buf.data() + offsets.TypeOffset);
      THROW_IF(state == DuckDBError, "Failed to append type");
      state = duckdb_append_varchar(appender, Buf.data() + offsets.FlagsOffset);
      THROW_IF(state == DuckDBError, "Failed to append flags");

      auto& nums(Nums[i]);
      state = duckdb_append_uint64(appender, nums.MetaAddr);
      THROW_IF(state == DuckDBError, "Failed to append meta_addr");
      state = duckdb_append_uint64(appender, nums.ParentAddr);
      THROW_IF(state == DuckDBError, "Failed to append parent_addr");
      state = duckdb_append_uint64(appender, nums.MetaSeq);
      THROW_IF(state == DuckDBError, "Failed to append meta_seq");
      state = duckdb_append_uint64(appender, nums.ParentSeq);
      THROW_IF(state == DuckDBError, "Failed to append parent_seq");
*/
    state = duckdb_appender_end_row(appender);
    THROW_IF(state == DuckDBError, "Failed call to end_row");
  }
  Buf.clear();
  Offsets.clear();
  Nums.clear();
}

