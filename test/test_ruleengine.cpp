#include <catch2/catch_test_macros.hpp>

#include "ruleengine.h"
#include "llamaduck.h"
#include "rulereader.h"
#include "inode.h"
#include "direntbatch.h"

TEST_CASE("TestCreateTables") {
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

TEST_CASE("TestWriteRuleToDb") {
  std::string input = "rule MyRule { file_metadata: created > \"2021-01-01\" } rule MyRule2 { file_metadata: filesize > 100 }";
  RuleReader reader;
  reader.read(input);
  RuleEngine engine;
  LlamaDB db;
  LlamaDBConnection conn(db);
  REQUIRE(DBType<Dirent>::createTable(conn.get(), "dirent"));
  REQUIRE(DBType<Inode>::createTable(conn.get(), "inode"));
  engine.createTables(conn);
  engine.writeRulesToDb(reader, conn);
  duckdb_result result;
  auto state = duckdb_query(conn.get(), "select * from rules;", &result);
  CHECK(state == DuckDBSuccess);
  CHECK(duckdb_result_error(&result) == nullptr);
  REQUIRE(duckdb_row_count(&result) == 2);
  state = duckdb_query(conn.get(), "select * from rule_matches;", &result);
  REQUIRE(state == DuckDBSuccess);
  REQUIRE(duckdb_row_count(&result) == 0);
}

TEST_CASE("TestZeroRulesToDb") {
  RuleReader reader;
  RuleEngine engine;
  LlamaDB    db;
  LlamaDBConnection conn(db);

  REQUIRE_NOTHROW(engine.createTables(conn));
  REQUIRE_NOTHROW(engine.writeRulesToDb(reader, conn));
}

TEST_CASE("getFsm") {
  std::string input = "rule myRule { grep: patterns: a = \"test\" encodings=UTF-8,UTF-16LE condition: any()}";
  RuleReader reader;
  reader.read(input);
  RuleEngine engine;
  LgFsmHolder fsmHolder = engine.getFsm(reader);
  REQUIRE(lg_fsm_pattern_count(fsmHolder.getFsm()) == 2);
}