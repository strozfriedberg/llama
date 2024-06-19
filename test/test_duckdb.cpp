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

  static bool createTable(duckdb_connection& dbconn, const std::string& table) {
    std::string query = "CREATE TABLE " + table + " (path VARCHAR, name VARCHAR);";
    return DuckDBSuccess == duckdb_query(dbconn, query.c_str(), nullptr);
  }

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

class LlamaDB {
public:
  LlamaDB(const char* path = nullptr) {
    auto state = duckdb_open(path, &Db);
    THROW_IF(state == DuckDBError, "Failed to open database");
  }

  duckdb_database& get() { return Db; }

  ~LlamaDB() {
    duckdb_close(&Db);
  }

private:
  duckdb_database Db;
};

class LlamaDBConnection {
public:
  LlamaDBConnection(LlamaDB& db) {
    auto state = duckdb_connect(db.get(), &DBConn);
    THROW_IF(state == DuckDBError, "Failed to connect to database");
  }
  
  ~LlamaDBConnection() {
    duckdb_disconnect(&DBConn);
  }

  duckdb_connection& get() { return DBConn; }

private:
  LlamaDBConnection(const LlamaDBConnection&) = delete;

  duckdb_connection DBConn;
};

class LlamaDBAppender {
public:
  LlamaDBAppender(duckdb_connection& conn, const char* table) {
    auto state = duckdb_appender_create(conn, nullptr, table, &Appender);
    THROW_IF(state == DuckDBError, "Failed to create appender");
  }

  ~LlamaDBAppender() {
    duckdb_appender_destroy(&Appender);
  }

  duckdb_appender& get() { return Appender; }

  bool flush() {
    return DuckDBSuccess == duckdb_appender_flush(Appender);
  }

private:
  duckdb_appender Appender;
};

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

