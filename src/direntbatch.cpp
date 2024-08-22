#include "direntbatch.h"
#include "llamaduck.h"

void appendVal(duckdb_appender& appender, const char* s) {
  duckdb_state state = duckdb_append_varchar(appender, s);
  THROW_IF(state == DuckDBError, "Failed to append string value");
}

void appendVal(duckdb_appender& appender, uint64_t val) {
  duckdb_state state = duckdb_append_uint64(appender, val);
  THROW_IF(state == DuckDBError, "Failed to append uint64 value");
}

