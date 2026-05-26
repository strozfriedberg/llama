#pragma once

#include "llamaduck.h"
#include "pluginmanager.h"
#include "plugin_api.h"

#include <string>
#include <unordered_map>
#include <vector>

class PluginTableWriter {
public:
  PluginTableWriter(LlamaDB* db, const std::vector<PluginTableMeta>& tableMeta);
  ~PluginTableWriter();

  PluginTableWriter(const PluginTableWriter&) = delete;
  PluginTableWriter& operator=(const PluginTableWriter&) = delete;

  // Convert an Arrow batch and append it to the named table.
  // Returns 0 on success, negative on error.
  int write(const char* tableName, struct ArrowSchema* schema, struct ArrowArray* array);

  // Flush pending data to the underlying tables without closing the
  // appenders. Safe to call repeatedly during processing; appenders
  // remain usable for further writes after a flush.
  void flush();

  // Flush and close all appenders. Call once after all processing is done.
  void close();

  // C ABI trampoline: cast opaque to PluginTableWriter* and call write().
  static int trampoline(void* opaque, const char* tableName,
                        struct ArrowSchema* schema, struct ArrowArray* array);

private:
  struct TableState {
    duckdb_appender appender;
    duckdb_arrow_converted_schema convertedSchema; // borrowed, not owned
  };

  LlamaDBConnection Conn;
  std::unordered_map<std::string, TableState> Tables;
  bool Closed = false;
};
