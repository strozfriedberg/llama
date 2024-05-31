#include <catch2/catch_test_macros.hpp>

#include <duckdb.h>

struct Dirent {
  std::string name;
  std::string path;
};

TEST_CASE("TestMakeDuckDB") { 
  duckdb_database db(nullptr);
  duckdb_connection dbconn(nullptr);

  REQUIRE(duckdb_open(nullptr, &db) == DuckDBSuccess);
  REQUIRE(duckdb_connect(db, &dbconn) == DuckDBSuccess);


  auto state = duckdb_query(dbconn, "CREATE TABLE dirent (name VARCHAR, path VARCHAR);", nullptr);
  REQUIRE(state != DuckDBError);

  std::vector<Dirent> dirents = {
    {"foo", "/tmp/"},
    {"bar", "/tmp/"},
    {"bar", "/temp/"}
  };

  std::string query;;
  for (auto dirent : dirents) {
    query = "INSERT INTO dirent VALUES ('" + dirent.name + "', '" + dirent.path + "');";
    state = duckdb_query(dbconn, query.c_str(), nullptr);
    REQUIRE(state != DuckDBError);
  }
  duckdb_result result;
  state = duckdb_query(dbconn, "SELECT * FROM dirent WHERE dirent.path = '/tmp/';", &result);
  REQUIRE(state != DuckDBError);
  REQUIRE(duckdb_row_count(&result) == 2);

  duckdb_disconnect(&dbconn);
  duckdb_close(&db);
}

