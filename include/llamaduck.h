#pragma once

#include "throw.h"

#include <duckdb.h>

#include <algorithm>
#include <string>
#include <tuple>
#include <vector>

#include <boost/pfr.hpp>

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
  LlamaDBAppender(duckdb_connection& conn, const std::string& table) {
    auto state = duckdb_appender_create(conn, nullptr, table.c_str(), &Appender);
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

  size_t addString(uint64_t offset, const std::string& s) {
    OffsetVals.push_back(offset);
    std::copy_n(s.begin(), s.size() + 1, Buf.begin() + OffsetVals.back());
    return offset + s.size() + 1;
  }

  void clear() {
    Buf.clear();
    OffsetVals.clear();
    NumRows = 0;
  }
};

template<typename T>
size_t addStrings(DuckBatch& batch, size_t offset, const T& s) {
  return batch.addString(offset, s);
}

template<typename T, typename... Args>
size_t addStrings(DuckBatch& batch, size_t offset, const T& s, Args... args) {
  batch.addString(offset, s);
  return addStrings(batch, offset + s.size() + 1, args...);
}

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

template<typename T>
size_t totalStringSize(const T& cur) {
  if constexpr (std::is_convertible_v<T, std::string>) {
    return cur.size() + 1;
  }
  else {
    return 0;
  }
}

template<typename T, typename... Args>
size_t totalStringSize(T cur, Args... others) {
  size_t totalSize = 0;
  if constexpr (std::is_convertible_v<T, std::string>) {
    totalSize += cur.size() + 1;
  }
  totalSize += totalStringSize(others...);
  return totalSize;
}

template<typename... Args>
size_t totalStringSize(const std::tuple<Args...>& tup) {
  size_t totalSize = 0;
  std::apply([&totalSize](auto&&... elem) {
    totalSize = (totalStringSize(elem) + ...);
  }, tup);
  return totalSize;
}

template<typename BaseStruct, typename... Args>
struct SchemaType : public BaseStruct {
  using TupleType = std::tuple<Args...>;

  static constexpr auto NumCols = std::tuple_size_v<TupleType>;

  static bool createTable(duckdb_connection& dbconn, const std::string& table) {
    duckdb_state state = duckdb_query(dbconn, createQuery<SchemaType>(table.c_str()).c_str(), nullptr);
    return state != DuckDBError;
  }

  static constexpr auto colIndex(const char* col) {
    unsigned int i = 0;
    const auto nameLen = std::char_traits<char>::length(col);
    for (auto name : BaseStruct::ColNames) {
      if (std::char_traits<char>::length(col) == nameLen && std::char_traits<char>::compare(col, name, nameLen) == 0) {
        break;
      }
      ++i;
    }
    return i;
  }

  SchemaType(): BaseStruct() {}
  SchemaType(const BaseStruct& base): BaseStruct(base) {}
};

