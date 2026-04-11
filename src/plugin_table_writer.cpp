#include "plugin_table_writer.h"

#include <iostream>

PluginTableWriter::PluginTableWriter(LlamaDB* db, const std::vector<PluginTableMeta>& tableMeta)
  : Conn(*db)
{
  for (const auto& tm : tableMeta) {
    duckdb_appender appender = nullptr;
    auto state = duckdb_appender_create(Conn.get(), nullptr, tm.tableName.c_str(), &appender);
    if (state == DuckDBError) {
      std::cerr << "Warning: failed to create appender for " << tm.tableName << "\n";
      continue;
    }
    Tables[tm.tableName] = {appender, tm.convertedSchema};
  }
}

PluginTableWriter::~PluginTableWriter() {
  for (auto& [name, ts] : Tables) {
    if (ts.appender) {
      duckdb_appender_destroy(&ts.appender);
    }
  }
}

int PluginTableWriter::write(const char* tableName, struct ArrowSchema* schema, struct ArrowArray* array) {
  (void)schema;  // schema carried by cached converted schema from init

  auto it = Tables.find(tableName);
  if (it == Tables.end()) {
    return -1;  // unknown table
  }

  duckdb_data_chunk chunk = nullptr;
  duckdb_error_data err = duckdb_data_chunk_from_arrow(
    Conn.get(), array, it->second.convertedSchema, &chunk
  );
  if (err) {
    std::cerr << "Warning: Arrow->DataChunk conversion failed for " << tableName
              << ": " << duckdb_error_data_message(err) << "\n";
    duckdb_destroy_error_data(&err);
    return -1;
  }

  auto state = duckdb_append_data_chunk(it->second.appender, chunk);
  duckdb_destroy_data_chunk(&chunk);

  if (state == DuckDBError) {
    std::cerr << "Warning: append failed for " << tableName << "\n";
    return -1;
  }

  return 0;
}

void PluginTableWriter::close() {
  if (Closed) return;
  Closed = true;
  for (auto& [name, ts] : Tables) {
    if (ts.appender) {
      duckdb_appender_close(ts.appender);
    }
  }
}

int PluginTableWriter::trampoline(void* opaque, const char* tableName,
                                  struct ArrowSchema* schema, struct ArrowArray* array) {
  return static_cast<PluginTableWriter*>(opaque)->write(tableName, schema, array);
}
