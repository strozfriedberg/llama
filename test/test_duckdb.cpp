#include <catch2/catch_test_macros.hpp>

#include <duckdb.hpp>

using namespace duckdb;

struct Dirent {
  std::string name;
  std::string path;
};

TEST_CASE("TestMakeDuckDB") {
  DuckDB db(nullptr);
  Connection dbconn(db);

  auto result = dbconn.Query("CREATE TABLE dirent (name VARCHAR, path VARCHAR);");
  REQUIRE(!result->HasError());

  std::vector<Dirent> dirents = {
    {"foo", "/tmp/"},
    {"bar", "/tmp/"},
    {"bar", "/temp/"}
  };

  for (auto dirent : dirents) {
    result = dbconn.Query("INSERT INTO dirent VALUES ('" + dirent.name + "', '" + dirent.path + "');");
    REQUIRE(!result->HasError());
  }
}

