#include <catch2/catch_test_macros.hpp>

#include "ruleengine.h"
#include "llamaduck.h"
#include "rulereader.h"
#include "inode.h"
#include "direntbatch.h"

TEST_CASE("TestCreateTables") {
  LlamaRuleEngine engine;
  LlamaDB db;
  LlamaDBConnection conn(db);
  engine.createTables(conn);
  duckdb_result result;
  auto state = duckdb_query(conn.get(), "select * from rules;", &result);
  REQUIRE(state == DuckDBSuccess);
  REQUIRE(duckdb_row_count(&result) == 0);
  state = duckdb_query(conn.get(), "select * from rule_hits;", &result);
  REQUIRE(state == DuckDBSuccess);
  REQUIRE(duckdb_row_count(&result) == 0);
  state = duckdb_query(conn.get(), "select * from search_hits;", &result);
  REQUIRE(state == DuckDBSuccess);
  REQUIRE(duckdb_row_count(&result) == 0);
  REQUIRE_THROWS(engine.createTables(conn));
}

TEST_CASE("TestWriteRuleToDb") {
  std::string input = "rule MyRule { file_metadata: created > \"2021-01-01\" } rule MyRule2 { file_metadata: filesize > 100 }";
  LlamaRuleEngine engine;
  engine.read(input, "test");
  LlamaDB db;
  LlamaDBConnection conn(db);
  REQUIRE(DBType<Dirent>::createTable(conn.get(), "dirent"));
  REQUIRE(DBType<Inode>::createTable(conn.get(), "inode"));
  engine.createTables(conn);
  engine.writeRulesToDb(conn);
  duckdb_result result;
  auto state = duckdb_query(conn.get(), "select * from rules;", &result);
  CHECK(state == DuckDBSuccess);
  CHECK(duckdb_result_error(&result) == nullptr);
  REQUIRE(duckdb_row_count(&result) == 2);
  state = duckdb_query(conn.get(), "select * from rule_hits;", &result);
  REQUIRE(state == DuckDBSuccess);
  REQUIRE(duckdb_row_count(&result) == 0);
}

TEST_CASE("TestZeroRulesToDb") {
  RuleReader reader;
  LlamaRuleEngine engine;
  LlamaDB    db;
  LlamaDBConnection conn(db);

  REQUIRE_NOTHROW(engine.createTables(conn));
  REQUIRE_NOTHROW(engine.writeRulesToDb(conn));
}

TEST_CASE("getFsm") {
  std::string input = R"(
  rule myRule {
    grep:
      patterns:
        a = "test" encodings=UTF-8,UTF-16LE
      condition:
        any()
    }
  rule MyOtherRule {
    grep:
      patterns:
        a = "foobar" fixed
      condition:
        any()
  })";
  LlamaRuleEngine engine;
  engine.read(input, "test");
  LgFsmHolder fsmHolder = engine.buildFsm();
  REQUIRE(lg_fsm_pattern_count(fsmHolder.getFsm()) == 3);
  auto patToRuleId = engine.patternToRuleId();
  REQUIRE(patToRuleId.size() == 3);
  REQUIRE(patToRuleId[0] == patToRuleId[1]);
  REQUIRE(patToRuleId[1] != patToRuleId[2]);
}