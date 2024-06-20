#pragma once

#include <duckdb.h>

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
