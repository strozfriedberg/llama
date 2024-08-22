#pragma once

#include "throw.h"

#include <duckdb.h>

class LlamaDB {
public:
  LlamaDB(const char* path = nullptr) {
    auto state = duckdb_open(path, &Db);
    THROW_IF(state == DuckDBError, "Failed to open database");
  }

  duckdb_database& get() { return Db; }

  ~LlamaDB() {
    duckdb_close(&Db);
  }

private:
  duckdb_database Db;
};

class LlamaDBConnection {
public:
  LlamaDBConnection(LlamaDB& db) {
    auto state = duckdb_connect(db.get(), &DBConn);
    THROW_IF(state == DuckDBError, "Failed to connect to database");
  }
  
  ~LlamaDBConnection() {
    duckdb_disconnect(&DBConn);
  }

  duckdb_connection& get() { return DBConn; }

private:
  LlamaDBConnection(const LlamaDBConnection&) = delete;

  duckdb_connection DBConn;
};

class LlamaDBAppender {
public:
  LlamaDBAppender(duckdb_connection& conn, const char* table) {
    auto state = duckdb_appender_create(conn, nullptr, table, &Appender);
    THROW_IF(state == DuckDBError, "Failed to create appender");
  }

  ~LlamaDBAppender() {
    duckdb_appender_destroy(&Appender);
  }

  duckdb_appender& get() { return Appender; }

  bool flush() {
    return DuckDBSuccess == duckdb_appender_flush(Appender);
  }

private:
  duckdb_appender Appender;
};

template<typename T>
constexpr const char* duckdbType() {
  if constexpr (std::is_integral_v<T>) {
    return "UBIGINT";
  }
  else if constexpr (std::is_convertible_v<T, std::string>) {
    return "VARCHAR";
  }
  else {
    return "UNKNOWN";
    //static_assert(false, "Could not convert type successfully");
  }
}

template<size_t CurIndex, size_t N, typename TupleType>
static constexpr void duckTypes(std::string& out, const std::initializer_list<const char*>& colNames) {
  out += std::data(colNames)[CurIndex];
  out += " ";
  out += duckdbType<std::tuple_element_t<CurIndex, TupleType>>();
  if constexpr (CurIndex + 1 < N) {
    out += ", ";
    duckTypes<CurIndex + 1, N, TupleType>(out, colNames);
  }
}

template<typename SchemaType>
static std::string createQuery(const char* table) {
  std::string query = "CREATE TABLE ";
  query += table;
  query += " (";
  duckTypes<0, std::tuple_size_v<typename SchemaType::TupleType>, typename SchemaType::TupleType>(query, SchemaType::ColNames);
  query += ");";
  return query;
}

void appendVal(duckdb_appender& appender, const char* s);
void appendVal(duckdb_appender& appender, uint64_t val);

template<typename T>
void append(duckdb_appender& appender, T t) {
  appendVal(appender, t);
}

template<typename T, typename... Args>
void append(duckdb_appender& appender, T t, Args... args) {
  appendVal(appender, t);
  append(appender, args...);
}

struct DuckBatch {
  size_t size() const { return NumRows; }

  std::vector<char>    Buf; // strings stored in sequence here  
  std::vector<uint64_t> OffsetVals; // offsets to strings OR uint64_t values

  uint64_t NumRows = 0;

  void clear() {
    Buf.clear();
    OffsetVals.clear();
    NumRows = 0;
  }
};

template<typename T>
void AppendBatchImpl(duckdb_appender& appender, DuckBatch& batch, size_t i) {
  if constexpr (std::is_integral_v<T>) {
    appendVal(appender, batch.OffsetVals[i]);
  }
  else if constexpr (std::is_convertible_v<T, std::string>){
    appendVal(appender, batch.Buf.data() + batch.OffsetVals[i]);
  }
}

template<typename T, typename... Args>
void appendBatchImpl(duckdb_appender& appender, DuckBatch& batch, size_t i) {
  if constexpr (std::is_integral_v<T>) {
    appendVal(appender, batch.OffsetVals[i]);
  }
  else if constexpr (std::is_convertible_v<T, std::string>){
    appendVal(appender, batch.Buf.data() + batch.OffsetVals[i]);
  }
  appendBatchImpl<Args...>(appender, batch, i + 1);
}

template<typename... Args>
struct SchemaType {
  using TupleType = std::tuple<Args...>;

  static constexpr auto NumCols = std::tuple_size_v<TupleType>;
};

