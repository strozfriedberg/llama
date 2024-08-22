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

template<typename T>
void append(duckdb_appender& appender, T t) {
  appendVal(appender, t);
}

template<typename T, typename... Args>
void append(duckdb_appender& appender, T t, Args... args) {
  appendVal(appender, t);
  append(appender, args...);
}

bool DirentBatch::createTable(duckdb_connection& dbconn, const std::string& table) {
  duckdb_state state = duckdb_query(dbconn, createQuery<Dirent>(table.c_str()).c_str(), nullptr);
  return state != DuckDBError;
}

void DirentBatch::copyToDB(duckdb_appender& appender) {
  duckdb_state state;
  for (unsigned int i = 0; i + 9 < OffsetVals.size(); i += 10) {
    append(appender,
           Buf.data() + OffsetVals[i],
           Buf.data() + OffsetVals[i + 1],
           Buf.data() + OffsetVals[i + 2],
           Buf.data() + OffsetVals[i + 3],
           Buf.data() + OffsetVals[i + 4],
           Buf.data() + OffsetVals[i + 5],
           OffsetVals[i + 6],
           OffsetVals[i + 7],
           OffsetVals[i + 8],
           OffsetVals[i + 9]
    );
    state = duckdb_appender_end_row(appender);
    THROW_IF(state == DuckDBError, "Failed call to end_row");
  }
  Buf.clear();
  OffsetVals.clear();
  NumRows = 0;
}

