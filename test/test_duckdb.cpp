#include <catch2/catch_test_macros.hpp>

#include "throw.h"

#include "direntbatch.h"
#include "llamaduck.h"


TEST_CASE("TestMakeDuckDB") { 
  LlamaDB db;
  LlamaDBConnection conn(db);

  REQUIRE(DirentBatch::createTable(conn.get(), "dirent"));

  std::vector<Dirent> dirents = {
    {"/tmp/", "foo"},
    {"/tmp/", "bar"},
    {"/temp/", "bar"}
  };

  DirentBatch batch;
  std::string query;;
  for (auto dirent : dirents) {
    batch.add(dirent);
  }
  REQUIRE(batch.size() == 3);
  REQUIRE(batch.Buf.size() == 28);

  LlamaDBAppender appender(conn.get(), "dirent"); // need an appender object, too, which also doesn't jibe with smart pointers, and destroy must be called even if create returns an error
  // REQUIRE(state != DuckDBError);
  batch.copyToDB(appender.get());
  REQUIRE(appender.flush());

  duckdb_result result;
  auto state = duckdb_query(conn.get(), "SELECT * FROM dirent WHERE dirent.path = '/tmp/';", &result);
  REQUIRE(state != DuckDBError);
  REQUIRE(duckdb_row_count(&result) == 2);
}

