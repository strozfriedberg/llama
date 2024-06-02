#include <catch2/catch_test_macros.hpp>

#include "throw.h"

#include <duckdb.h>

struct Dirent {
  std::string path;
  std::string name;
};

struct DirentBatch {
  size_t size() const { return Offsets.size(); }

  std::vector<char> Buf;
  std::vector<std::pair<size_t, size_t>> Offsets;

  void add(const Dirent& dent) {
    size_t pathSize = dent.path.size();
    size_t totalSize = pathSize + dent.name.size();
    size_t offset = Buf.size();
    Buf.resize(offset + totalSize + 1);
    Offsets.push_back({offset, offset + pathSize});
    std::copy(dent.path.begin(), dent.path.end(), Buf.begin() + Offsets.back().first);
    std::copy(dent.name.begin(), dent.name.end(), Buf.begin() + Offsets.back().second);
    Buf[offset + totalSize] = '\0';
  }

  void copyToDB(duckdb_appender& appender) {
    duckdb_state state;
    for (auto& offsets: Offsets) {
      state = duckdb_append_varchar_length(appender, Buf.data() + offsets.first, offsets.second - offsets.first);
      THROW_IF(state == DuckDBError, "Failed to append path");
      state = duckdb_append_varchar(appender, Buf.data() + offsets.second);
      THROW_IF(state == DuckDBError, "Failed to append name");
      state = duckdb_appender_end_row(appender);
      THROW_IF(state == DuckDBError, "Failed call to end_row");
    }
  }
};

TEST_CASE("TestMakeDuckDB") { 
  duckdb_database db(nullptr);
  duckdb_connection dbconn(nullptr);

  REQUIRE(duckdb_open(nullptr, &db) == DuckDBSuccess);
  REQUIRE(duckdb_connect(db, &dbconn) == DuckDBSuccess);

  auto state = duckdb_query(dbconn, "CREATE TABLE dirent (path VARCHAR, name VARCHAR);", nullptr);
  REQUIRE(state != DuckDBError);

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

  duckdb_appender appender;
  state = duckdb_appender_create(dbconn, nullptr, "dirent", &appender);
  REQUIRE(state != DuckDBError);
  batch.copyToDB(appender);
  duckdb_appender_destroy(&appender);

  duckdb_result result;
  state = duckdb_query(dbconn, "SELECT * FROM dirent WHERE dirent.path = '/tmp/';", &result);
  REQUIRE(state != DuckDBError);
  REQUIRE(duckdb_row_count(&result) == 2);

  duckdb_disconnect(&dbconn);
  duckdb_close(&db);
}

