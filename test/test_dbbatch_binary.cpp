#include <catch2/catch_test_macros.hpp>
#include <array>
#include <optional>
#include <duckdb.h>
#include "llamaduck.h"

namespace {

struct BinFirst {
  static constexpr auto ColNames = {"id", "name"};
  std::array<uint8_t, 32> id;
  std::string name;
};

struct BinLast {
  static constexpr auto ColNames = {"name", "id"};
  std::string name;
  std::array<uint8_t, 32> id;
};

struct BinEnds {
  static constexpr auto ColNames = {"id", "name", "value", "tag"};
  std::array<uint8_t, 16> id;
  std::string name;
  uint64_t value;
  std::array<uint8_t, 32> tag;
};

struct Alternating {
  static constexpr auto ColNames = {"a", "x", "b", "y"};
  std::array<uint8_t, 8> a;
  uint64_t x;
  std::array<uint8_t, 8> b;
  uint64_t y;
};

struct AllBinary {
  static constexpr auto ColNames = {"a", "b"};
  std::array<uint8_t, 8> a;
  std::array<uint8_t, 16> b;
};

struct OptAndReq {
  static constexpr auto ColNames = {"req", "opt"};
  std::array<uint8_t, 32> req;
  std::optional<std::array<uint8_t, 16>> opt;
};

struct MultiOpt {
  static constexpr auto ColNames = {"id", "opt_a", "opt_b"};
  std::array<uint8_t, 32> id;
  std::optional<std::array<uint8_t, 16>> opt_a;
  std::optional<std::array<uint8_t, 32>> opt_b;
};

template<typename Rec>
struct DuckScope {
  duckdb_database  db{};
  duckdb_connection conn{};
  duckdb_appender   app{};

  DuckScope(const char* table) {
    REQUIRE(duckdb_open(nullptr, &db) == DuckDBSuccess);
    REQUIRE(duckdb_connect(db, &conn) == DuckDBSuccess);
    REQUIRE(DBType<Rec>::createTable(conn, table));
    REQUIRE(duckdb_appender_create(conn, nullptr, table, &app) == DuckDBSuccess);
  }
  ~DuckScope() {
    duckdb_appender_destroy(&app);
    duckdb_disconnect(&conn);
    duckdb_close(&db);
  }
};

}  // namespace

TEST_CASE("DBBatch round-trips binary-first record", "[dbbatch][binary]") {
  DuckScope<BinFirst> scope("t");
  DBBatch<BinFirst> batch;
  BinFirst row{};
  for (size_t i = 0; i < 32; ++i) row.id[i] = uint8_t(i);
  row.name = "hello";
  batch.add(row);
  REQUIRE(batch.copyToDB(scope.app) == 1);
  REQUIRE(duckdb_appender_flush(scope.app) == DuckDBSuccess);
  duckdb_result r;
  REQUIRE(duckdb_query(scope.conn,
    "SELECT typeof(id), octet_length(id), typeof(name) FROM t", &r) == DuckDBSuccess);
  REQUIRE(std::string(duckdb_value_varchar_internal(&r, 0, 0)) == "BLOB");
  REQUIRE(duckdb_value_int64(&r, 1, 0) == 32);
  REQUIRE(std::string(duckdb_value_varchar_internal(&r, 2, 0)) == "VARCHAR");
  duckdb_destroy_result(&r);
}

TEST_CASE("DBBatch round-trips binary-last record", "[dbbatch][binary]") {
  DuckScope<BinLast> scope("t");
  DBBatch<BinLast> batch;
  BinLast row{};
  row.name = "x";
  for (size_t i = 0; i < 32; ++i) row.id[i] = uint8_t(0xA0 | (i & 0x0F));
  batch.add(row);
  REQUIRE(batch.copyToDB(scope.app) == 1);
  REQUIRE(duckdb_appender_flush(scope.app) == DuckDBSuccess);
  duckdb_result r;
  REQUIRE(duckdb_query(scope.conn,
    "SELECT octet_length(id), hex(id) FROM t", &r) == DuckDBSuccess);
  REQUIRE(duckdb_value_int64(&r, 0, 0) == 32);
  duckdb_destroy_result(&r);
}

TEST_CASE("DBBatch round-trips binary-at-both-ends with middle non-binary", "[dbbatch][binary]") {
  DuckScope<BinEnds> scope("t");
  DBBatch<BinEnds> batch;
  for (int i = 0; i < 3; ++i) {
    BinEnds row{};
    for (size_t j = 0; j < 16; ++j) row.id[j] = uint8_t(i * 16 + j);
    row.name = "row" + std::to_string(i);
    row.value = uint64_t(i * 100);
    for (size_t j = 0; j < 32; ++j) row.tag[j] = uint8_t(0xC0 | (j & 0x0F));
    batch.add(row);
  }
  REQUIRE(batch.copyToDB(scope.app) == 3);
  REQUIRE(duckdb_appender_flush(scope.app) == DuckDBSuccess);
  duckdb_result r;
  REQUIRE(duckdb_query(scope.conn,
    "SELECT count(*), sum(value) FROM t", &r) == DuckDBSuccess);
  REQUIRE(duckdb_value_int64(&r, 0, 0) == 3);
  REQUIRE(duckdb_value_int64(&r, 1, 0) == 300);
  duckdb_destroy_result(&r);
  REQUIRE(duckdb_query(scope.conn,
    "SELECT octet_length(id), octet_length(tag) FROM t LIMIT 1", &r) == DuckDBSuccess);
  REQUIRE(duckdb_value_int64(&r, 0, 0) == 16);
  REQUIRE(duckdb_value_int64(&r, 1, 0) == 32);
  duckdb_destroy_result(&r);
}

TEST_CASE("DBBatch round-trips alternating binary/non-binary", "[dbbatch][binary]") {
  DuckScope<Alternating> scope("t");
  DBBatch<Alternating> batch;
  Alternating row{};
  for (size_t j = 0; j < 8; ++j) { row.a[j] = uint8_t(j); row.b[j] = uint8_t(j + 100); }
  row.x = 42; row.y = 999;
  batch.add(row);
  REQUIRE(batch.copyToDB(scope.app) == 1);
  REQUIRE(duckdb_appender_flush(scope.app) == DuckDBSuccess);
  duckdb_result r;
  REQUIRE(duckdb_query(scope.conn,
    "SELECT octet_length(a), x, octet_length(b), y FROM t", &r) == DuckDBSuccess);
  REQUIRE(duckdb_value_int64(&r, 1, 0) == 42);
  REQUIRE(duckdb_value_int64(&r, 3, 0) == 999);
  duckdb_destroy_result(&r);
}

TEST_CASE("DBBatch round-trips all-binary record", "[dbbatch][binary]") {
  DuckScope<AllBinary> scope("t");
  DBBatch<AllBinary> batch;
  AllBinary row{};
  for (size_t j = 0; j < 8; ++j) row.a[j] = uint8_t(j);
  for (size_t j = 0; j < 16; ++j) row.b[j] = uint8_t(j);
  batch.add(row);
  REQUIRE(batch.copyToDB(scope.app) == 1);
  REQUIRE(duckdb_appender_flush(scope.app) == DuckDBSuccess);
  duckdb_result r;
  REQUIRE(duckdb_query(scope.conn,
    "SELECT octet_length(a), octet_length(b) FROM t", &r) == DuckDBSuccess);
  REQUIRE(duckdb_value_int64(&r, 0, 0) == 8);
  REQUIRE(duckdb_value_int64(&r, 1, 0) == 16);
  duckdb_destroy_result(&r);
}

TEST_CASE("DBBatch round-trips optional alongside required binary", "[dbbatch][binary]") {
  DuckScope<OptAndReq> scope("t");
  DBBatch<OptAndReq> batch;
  OptAndReq present{};
  for (size_t j = 0; j < 32; ++j) present.req[j] = uint8_t(0x11);
  std::array<uint8_t, 16> v{};
  for (size_t j = 0; j < 16; ++j) v[j] = uint8_t(0x22);
  present.opt = v;
  batch.add(present);
  OptAndReq absent{};
  for (size_t j = 0; j < 32; ++j) absent.req[j] = uint8_t(0x33);
  absent.opt = std::nullopt;
  batch.add(absent);
  REQUIRE(batch.copyToDB(scope.app) == 2);
  REQUIRE(duckdb_appender_flush(scope.app) == DuckDBSuccess);
  duckdb_result r;
  REQUIRE(duckdb_query(scope.conn,
    "SELECT octet_length(req), opt IS NULL FROM t ORDER BY req", &r) == DuckDBSuccess);
  REQUIRE(duckdb_value_int64(&r, 0, 0) == 32);  // req[0] = 0x11
  REQUIRE(duckdb_value_int64(&r, 1, 0) == 0);   // opt present
  REQUIRE(duckdb_value_int64(&r, 1, 1) == 1);   // opt null
  duckdb_destroy_result(&r);
}

TEST_CASE("DBBatch handles multiple optional binary columns with mixed presence", "[dbbatch][binary]") {
  DuckScope<MultiOpt> scope("t");
  DBBatch<MultiOpt> batch;
  for (int i = 0; i < 4; ++i) {
    MultiOpt row{};
    for (size_t j = 0; j < 32; ++j) row.id[j] = uint8_t(i);
    if (i & 1) {
      std::array<uint8_t, 16> a{};
      for (auto& b : a) b = uint8_t(0xAA);
      row.opt_a = a;
    }
    if (i & 2) {
      std::array<uint8_t, 32> b{};
      for (auto& by : b) by = uint8_t(0xBB);
      row.opt_b = b;
    }
    batch.add(row);
  }
  REQUIRE(batch.copyToDB(scope.app) == 4);
  REQUIRE(duckdb_appender_flush(scope.app) == DuckDBSuccess);
  duckdb_result r;
  REQUIRE(duckdb_query(scope.conn,
    "SELECT sum((opt_a IS NULL)::INT), sum((opt_b IS NULL)::INT) FROM t", &r) == DuckDBSuccess);
  REQUIRE(duckdb_value_int64(&r, 0, 0) == 2);  // rows 0, 2
  REQUIRE(duckdb_value_int64(&r, 1, 0) == 2);  // rows 0, 1
  duckdb_destroy_result(&r);
}
