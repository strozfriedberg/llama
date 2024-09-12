#include <catch2/catch_test_macros.hpp>

#include "ruleengine.h"
#include "llamaduck.h"

TEST_CASE("TestRuleEngine") {
  RuleEngine engine;
  LlamaDB db;
  LlamaDBConnection conn(db);
  engine.createTables(conn);
  duckdb_result result;
  auto state = duckdb_query(conn.get(), "select * from rules;", &result);
  REQUIRE(state == DuckDBSuccess);
  REQUIRE(duckdb_row_count(&result) == 0);
  REQUIRE_THROWS(engine.createTables(conn));
}