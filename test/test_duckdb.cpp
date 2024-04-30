#include <catch2/catch_test_macros.hpp>

#include <duckdb.hpp>

using namespace duckdb;

TEST_CASE("TestMakeDuckDB") {
  DuckDB db(nullptr);
  Connection dbconn(db);
}
