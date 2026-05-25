#pragma once

#include "bitsetvar.h"
#include "throw.h"

#include <duckdb.h>

#include <algorithm>
#include <array>
#include <optional>
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

// Traits: byte array detection (must precede duckdbType)
template<typename T> struct is_byte_array : std::false_type {};
template<size_t N>   struct is_byte_array<std::array<uint8_t, N>> : std::true_type {};
template<typename T> inline constexpr bool is_byte_array_v = is_byte_array<T>::value;

template<typename T> struct is_optional_byte_array : std::false_type {};
template<size_t N>   struct is_optional_byte_array<std::optional<std::array<uint8_t, N>>> : std::true_type {};
template<typename T> inline constexpr bool is_optional_byte_array_v = is_optional_byte_array<T>::value;

template<typename T>
constexpr const char* duckdbType() {
  if constexpr (is_byte_array_v<T> || is_optional_byte_array_v<T>) {
    return "BLOB";
  }
  else if constexpr (std::is_integral_v<T>) {
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
void appendVal(duckdb_appender& appender, const uint8_t* data, size_t len);

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

template<typename TupleType, size_t I = 0>
constexpr size_t numNonBinaryCols() {
  if constexpr (I >= std::tuple_size_v<TupleType>) return 0;
  else {
    using C = std::tuple_element_t<I, TupleType>;
    if constexpr (is_byte_array_v<C> || is_optional_byte_array_v<C>)
      return numNonBinaryCols<TupleType, I + 1>();
    else
      return 1 + numNonBinaryCols<TupleType, I + 1>();
  }
}

template<typename TupleType, size_t CurIndex, size_t I = 0>
constexpr size_t nonBinaryColIndex() {
  static_assert(CurIndex <= std::tuple_size_v<TupleType>);
  if constexpr (I >= CurIndex) return 0;
  else {
    using C = std::tuple_element_t<I, TupleType>;
    if constexpr (is_byte_array_v<C> || is_optional_byte_array_v<C>)
      return nonBinaryColIndex<TupleType, CurIndex, I + 1>();
    else
      return 1 + nonBinaryColIndex<TupleType, CurIndex, I + 1>();
  }
}

// Step 1: binary-column constexpr helpers

template<typename T> struct byte_array_size : std::integral_constant<size_t, 0> {};
template<size_t N>   struct byte_array_size<std::array<uint8_t, N>> : std::integral_constant<size_t, N> {};
template<typename T> inline constexpr size_t byte_array_size_v = byte_array_size<T>::value;

template<typename TupleType, size_t I = 0>
constexpr size_t numBinaryCols() {
  if constexpr (I >= std::tuple_size_v<TupleType>) return 0;
  else {
    using C = std::tuple_element_t<I, TupleType>;
    if constexpr (is_byte_array_v<C> || is_optional_byte_array_v<C>)
      return 1 + numBinaryCols<TupleType, I + 1>();
    else
      return numBinaryCols<TupleType, I + 1>();
  }
}

template<typename TupleType, size_t CurIndex, size_t I = 0>
constexpr size_t binaryColIndex() {
  static_assert(CurIndex <= std::tuple_size_v<TupleType>);
  if constexpr (I >= CurIndex) return 0;
  else {
    using C = std::tuple_element_t<I, TupleType>;
    if constexpr (is_byte_array_v<C> || is_optional_byte_array_v<C>)
      return 1 + binaryColIndex<TupleType, CurIndex, I + 1>();
    else
      return binaryColIndex<TupleType, CurIndex, I + 1>();
  }
}

template<typename TupleType, size_t I = 0>
constexpr size_t numNullableBinaryCols() {
  if constexpr (I >= std::tuple_size_v<TupleType>) return 0;
  else {
    using C = std::tuple_element_t<I, TupleType>;
    if constexpr (is_optional_byte_array_v<C>)
      return 1 + numNullableBinaryCols<TupleType, I + 1>();
    else
      return numNullableBinaryCols<TupleType, I + 1>();
  }
}

template<typename TupleType, size_t CurIndex, size_t I = 0>
constexpr size_t nullableBinaryColIndex() {
  static_assert(CurIndex <= std::tuple_size_v<TupleType>);
  if constexpr (I >= CurIndex) return 0;
  else {
    using C = std::tuple_element_t<I, TupleType>;
    if constexpr (is_optional_byte_array_v<C>)
      return 1 + nullableBinaryColIndex<TupleType, CurIndex, I + 1>();
    else
      return nullableBinaryColIndex<TupleType, CurIndex, I + 1>();
  }
}

template<typename TupleType, size_t I = 0>
constexpr size_t rowBinaryBytes() {
  if constexpr (I >= std::tuple_size_v<TupleType>) return 0;
  else {
    using C = std::tuple_element_t<I, TupleType>;
    if constexpr (is_byte_array_v<C>)
      return byte_array_size_v<C> + rowBinaryBytes<TupleType, I + 1>();
    else if constexpr (is_optional_byte_array_v<C>)
      return byte_array_size_v<typename C::value_type> + rowBinaryBytes<TupleType, I + 1>();
    else
      return rowBinaryBytes<TupleType, I + 1>();
  }
}

// J is a binary-column ordinal (0 = first binary col in tuple order).
template<typename TupleType, size_t J, size_t I = 0, size_t Seen = 0>
constexpr size_t binaryColOffset() {
  if constexpr (I >= std::tuple_size_v<TupleType>) return 0;
  else {
    using C = std::tuple_element_t<I, TupleType>;
    if constexpr (is_byte_array_v<C>) {
      if constexpr (Seen == J) return 0;
      else return byte_array_size_v<C> + binaryColOffset<TupleType, J, I + 1, Seen + 1>();
    }
    else if constexpr (is_optional_byte_array_v<C>) {
      if constexpr (Seen == J) return 0;
      else return byte_array_size_v<typename C::value_type> + binaryColOffset<TupleType, J, I + 1, Seen + 1>();
    }
    else {
      return binaryColOffset<TupleType, J, I + 1, Seen>();
    }
  }
}

template<typename T>
struct DBBatch {
  size_t size() const { return NumRows; }

  std::vector<char>    Buf; // strings stored in sequence here
  std::vector<uint64_t> OffsetVals; // offsets to strings OR uint64_t values
  std::vector<uint8_t> BinaryBuf;   // R * rowBinaryBytes<TupleType>(), zero-filled for nulls
  BitsetVar            BinaryNull;  // R * numNullableBinaryCols<TupleType>() bits

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
    BinaryBuf.clear();
    BinaryNull.clear();
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
    else if constexpr (is_byte_array_v<Cur>) {
      BinaryBuf.insert(BinaryBuf.end(), cur.begin(), cur.end());
      // No BinaryNull push: non-optional columns are NOT NULL by type.
    }
    else if constexpr (is_optional_byte_array_v<Cur>) {
      constexpr size_t N = byte_array_size_v<typename Cur::value_type>;
      if (cur.has_value()) {
        BinaryBuf.insert(BinaryBuf.end(), cur->begin(), cur->end());
        BinaryNull.push_back(false);
      } else {
        BinaryBuf.insert(BinaryBuf.end(), N, uint8_t(0));
        BinaryNull.push_back(true);
      }
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
  void appendRecord(duckdb_appender& appender, size_t row) {
    using TupleType = typename DBType<T>::TupleType;
    using ColumnType = typename std::tuple_element<CurIndex, TupleType>::type;

    if constexpr (CurIndex > 0) {
      appendRecord<CurIndex - 1>(appender, row);
    }

    if constexpr (is_byte_array_v<ColumnType>) {
      constexpr size_t N = byte_array_size_v<ColumnType>;
      constexpr size_t colOffset = binaryColOffset<TupleType, binaryColIndex<TupleType, CurIndex>()>();
      appendVal(appender,
                BinaryBuf.data() + row * rowBinaryBytes<TupleType>() + colOffset,
                N);
    }
    else if constexpr (is_optional_byte_array_v<ColumnType>) {
      constexpr size_t N = byte_array_size_v<typename ColumnType::value_type>;
      constexpr size_t colOffset = binaryColOffset<TupleType, binaryColIndex<TupleType, CurIndex>()>();
      const size_t bit = row * numNullableBinaryCols<TupleType>() + nullableBinaryColIndex<TupleType, CurIndex>();
      if (BinaryNull.get(bit)) {
        duckdb_append_null(appender);
      } else {
        appendVal(appender,
                  BinaryBuf.data() + row * rowBinaryBytes<TupleType>() + colOffset,
                  N);
      }
    }
    else if constexpr (std::is_integral_v<ColumnType>) {
      const size_t slot = row * numNonBinaryCols<TupleType>() + nonBinaryColIndex<TupleType, CurIndex>();
      appendVal(appender, OffsetVals[slot]);
    }
    else if constexpr (std::is_convertible_v<ColumnType, std::string>) {
      const size_t slot = row * numNonBinaryCols<TupleType>() + nonBinaryColIndex<TupleType, CurIndex>();
      appendVal(appender, Buf.data() + OffsetVals[slot]);
    }
  }

  size_t copyToDB(duckdb_appender& appender) {
    for (size_t row = 0; row < NumRows; ++row) {
      duckdb_appender_begin_row(appender);
      appendRecord<DBType<T>::NumCols - 1>(appender, row);
      duckdb_appender_end_row(appender);
    }
    return NumRows;
  }
};
