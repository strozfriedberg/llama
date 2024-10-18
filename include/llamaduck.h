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

template<size_t CurIndex, size_t N>
static constexpr auto findIndex(const char* col, const std::initializer_list<const char*>& colNames) {
  auto curCol = std::data(colNames)[CurIndex];
  auto curLen = std::char_traits<char>::length(curCol);
  if (curLen == std::char_traits<char>::length(col) && std::char_traits<char>::compare(col, curCol, curLen) == 0) {
    return CurIndex;
  }
  else if constexpr (CurIndex + 1 < N) {
    return findIndex<CurIndex + 1, N>(col, colNames);
  }
  else {
    return N;
  }
}

template<typename T>
struct DBType {
  typedef T BaseType;

  typedef decltype(boost::pfr::structure_to_tuple(T())) TupleType; // this requires default constructor... come back to it

  static constexpr auto& ColNames = T::ColNames;

  static constexpr auto NumCols = std::tuple_size_v<TupleType>;

  static_assert(ColNames.size() == NumCols, "The list of column names must match the number of fields in the tuple.");

  static constexpr auto colIndex(const char* col) {
    return findIndex<0, T::ColNames.size()>(col, T::ColNames);
  }

  static bool createTable(duckdb_connection& dbconn, const std::string& table) {
    auto result = duckdb_query(dbconn, createQuery<DBType<T>>(table.c_str()).c_str(), nullptr);
    return result != DuckDBError;
  }
};


template<typename T>
struct DBBatch {
  size_t size() const { return NumRows; }

  std::vector<char>    Buf; // strings stored in sequence here
  std::vector<uint64_t> OffsetVals; // offsets to strings OR uint64_t values

  uint64_t NumRows = 0;

  void addString(size_t& offset, const std::string& s) {
    OffsetVals.push_back(offset);
    std::copy_n(s.begin(), s.size() + 1, Buf.begin() + OffsetVals.back());
    offset += s.size();
    ++offset; // for the null terminator
  }

  void clear() {
    Buf.clear();
    OffsetVals.clear();
    NumRows = 0;
  }

  template<typename Cur>
  void add(size_t& offset, const Cur& cur) {
    if constexpr (std::is_convertible<Cur, std::string>()) {
      addString(offset, cur);
    }
    else if constexpr (std::is_integral_v<Cur>) {
      OffsetVals.push_back(cur);
    }
  }

  void add(const T& t) {
    auto&& tupes = boost::pfr::structure_tie(t);
    size_t startOffset = Buf.size();
    size_t totalSize = totalStringSize(tupes);
    Buf.resize(startOffset + totalSize);

    std::apply([&](auto&&... car) { // "cons car cdr"
      (add(startOffset, car), ...);
    }, tupes);

    ++NumRows;
  }

  template<size_t CurIndex>
  void appendRecord(duckdb_appender& appender, size_t index) {
    using ColumnType = typename std::tuple_element<CurIndex, typename DBType<T>::TupleType>::type;
    if constexpr (CurIndex > 0) {
      appendRecord<CurIndex - 1>(appender, index - 1);
    }
    if constexpr (std::is_integral_v<ColumnType>) {
      appendVal(appender, OffsetVals[index]);
    }
    else if constexpr (std::is_convertible_v<ColumnType, std::string>){
      appendVal(appender, Buf.data() + OffsetVals[index]);
    }
  }

  size_t copyToDB(duckdb_appender& appender) {
    size_t index = 0;
    for (size_t i = 0; i < NumRows; ++i) {
      duckdb_appender_begin_row(appender);

      appendRecord<DBType<T>::NumCols - 1>(appender, index + DBType<T>::NumCols - 1);

      duckdb_appender_end_row(appender);
      index += DBType<T>::NumCols;
    }
    return NumRows;
  }
};

