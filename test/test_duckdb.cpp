#include <catch2/catch_test_macros.hpp>

#include "throw.h"

#include "direntbatch.h"
#include "llamaduck.h"


TEST_CASE("TestMakeDuckDB") { 
  LlamaDB db;
  LlamaDBConnection conn(db);

  REQUIRE(DirentBatch::createTable(conn.get(), "dirent"));

  std::vector<Dirent> dirents = {
    {"/tmp/", "foo", "f~1", "File", "Allocated", 3, 2, 0, 0},
    {"/tmp/", "bar", "b~1", "File", "Deleted", 4, 2, 0, 0},
    {"/temp/", "bar", "b~2", "File", "Allocated", 6, 5, 0, 0}
  };

  DirentBatch batch;
  std::string query;;
  for (auto dirent : dirents) {
    batch.add(dirent);
  }
  REQUIRE(batch.size() == dirents.size());
  REQUIRE(batch.Buf.size() == 86);

  LlamaDBAppender appender(conn.get(), "dirent"); // need an appender object, too, which also doesn't jibe with smart pointers, and destroy must be called even if create returns an error
  // REQUIRE(state != DuckDBError);
  batch.copyToDB(appender.get());
  REQUIRE(appender.flush());

  duckdb_result result;
  auto state = duckdb_query(conn.get(), "SELECT * FROM dirent WHERE dirent.path = '/tmp/' and ((dirent.name = 'bar' and dirent.meta_addr = 4) or (dirent.shrt_name = 'f~1' and dirent.par_addr = 2));", &result);
  REQUIRE(state != DuckDBError);
  REQUIRE(duckdb_row_count(&result) == 2);
  REQUIRE(duckdb_column_count(&result) == 9);
  REQUIRE(std::string("path") == duckdb_column_name(&result, 0));
  REQUIRE(std::string("name") == duckdb_column_name(&result, 1));
  REQUIRE(std::string("shrt_name") == duckdb_column_name(&result, 2));
  REQUIRE(std::string("type") == duckdb_column_name(&result, 3));
  REQUIRE(std::string("flags") == duckdb_column_name(&result, 4));
  REQUIRE(std::string("meta_addr") == duckdb_column_name(&result, 5));
  REQUIRE(std::string("par_addr") == duckdb_column_name(&result, 6));
  REQUIRE(std::string("meta_seq") == duckdb_column_name(&result, 7));
  REQUIRE(std::string("par_seq") == duckdb_column_name(&result, 8));
}

