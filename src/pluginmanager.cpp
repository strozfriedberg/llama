#include "pluginmanager.h"

#include <cstring>
#include <iostream>

namespace {

const char* arrowFormatToDuckDBType(const char* format) {
  if (!format) return nullptr;
  if (std::strcmp(format, "b") == 0) return "BOOLEAN";
  if (std::strcmp(format, "c") == 0) return "TINYINT";
  if (std::strcmp(format, "C") == 0) return "UTINYINT";
  if (std::strcmp(format, "s") == 0) return "SMALLINT";
  if (std::strcmp(format, "S") == 0) return "USMALLINT";
  if (std::strcmp(format, "i") == 0) return "INTEGER";
  if (std::strcmp(format, "I") == 0) return "UINTEGER";
  if (std::strcmp(format, "l") == 0) return "BIGINT";
  if (std::strcmp(format, "L") == 0) return "UBIGINT";
  if (std::strcmp(format, "f") == 0) return "FLOAT";
  if (std::strcmp(format, "g") == 0) return "DOUBLE";
  if (std::strcmp(format, "u") == 0) return "VARCHAR";
  if (std::strcmp(format, "U") == 0) return "VARCHAR";
  if (std::strcmp(format, "z") == 0) return "BLOB";
  if (std::strcmp(format, "Z") == 0) return "BLOB";
  return nullptr;
}

std::string quoteIdentifier(const char* name) {
  std::string q = "\"";
  for (const char* p = name; *p; ++p) {
    if (*p == '"') q += "\"\"";
    else q += *p;
  }
  q += "\"";
  return q;
}

std::string buildCreateTableSQL(const char* tableName, const struct ArrowSchema& schema) {
  std::string sql = "CREATE TABLE ";
  sql += quoteIdentifier(tableName);
  sql += " (";
  for (int64_t c = 0; c < schema.n_children; ++c) {
    if (c > 0) sql += ", ";
    sql += quoteIdentifier(schema.children[c]->name);
    sql += " ";
    const char* duckType = arrowFormatToDuckDBType(schema.children[c]->format);
    if (!duckType) return {};  // unsupported type
    sql += duckType;
  }
  sql += ");";
  return sql;
}

} // anonymous namespace

constexpr const char* PluginManager::pluginExtension() {
#if defined(__APPLE__)
  return ".dylib";
#elif defined(_WIN32)
  return ".dll";
#else
  return ".so";
#endif
}

PluginManager::~PluginManager() {
  shutdown();
}

void PluginManager::loadPlugins(const std::filesystem::path& pluginDir, duckdb_connection dbConn) {
  namespace fs = std::filesystem;
  const std::string ext = pluginExtension();

  for (const auto& entry : fs::directory_iterator(pluginDir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    if (entry.path().extension().string() != ext) {
      continue;
    }

    try {
      boost::dll::shared_library lib(entry.path().string());

      if (!lib.has("llama_plugin_init") ||
          !lib.has("llama_plugin_process") ||
          !lib.has("llama_plugin_free_error") ||
          !lib.has("llama_plugin_shutdown")) {
        std::cerr << "Warning: " << entry.path().filename()
                  << " missing required symbols, skipping\n";
        continue;
      }

      auto initFn    = lib.get<int(LlamaPluginInfo*, const LlamaTableDef**, size_t*)>("llama_plugin_init");
      auto processFn = lib.get<int(const LlamaFileContext*, const LlamaWriteContext*, const char**)>("llama_plugin_process");
      auto freeFn    = lib.get<void(const char*)>("llama_plugin_free_error");
      auto shutdownFn = lib.get<void()>("llama_plugin_shutdown");

      LlamaPluginInfo info{};
      const LlamaTableDef* tables = nullptr;
      size_t numTables = 0;
      if (initFn(&info, &tables, &numTables) < 0) {
        std::cerr << "Warning: " << entry.path().filename()
                  << " init failed, skipping\n";
        continue;
      }

      if (!info.name) {
        std::cerr << "Warning: " << entry.path().filename()
                  << " init returned null name, skipping\n";
        continue;
      }

      LoadedPlugin plugin;
      plugin.name = info.name;
      plugin.version = info.version ? info.version : "unknown";
      plugin.library = std::move(lib);
      plugin.process = processFn;
      plugin.freeError = freeFn;
      plugin.shutdown = shutdownFn;

      bool tablesOk = true;
      for (size_t t = 0; t < numTables; ++t) {
        const auto& tableDef = tables[t];
        std::string sql = buildCreateTableSQL(tableDef.table_name, tableDef.schema);
        if (sql.empty()) {
          std::cerr << "Warning: " << entry.path().filename()
                    << " declares table " << tableDef.table_name
                    << " with unsupported Arrow types, skipping plugin\n";
          tablesOk = false;
          break;
        }

        if (duckdb_query(dbConn, sql.c_str(), nullptr) == DuckDBError) {
          std::cerr << "Warning: " << entry.path().filename()
                    << " failed to create table " << tableDef.table_name
                    << ", skipping plugin\n";
          tablesOk = false;
          break;
        }

        duckdb_arrow_converted_schema converted = nullptr;
        // Shallow copy: duckdb_schema_from_arrow reads the schema structure
        // but does not call release on children. Plugin's static schemas
        // remain valid until shutdown.
        struct ArrowSchema schemaCopy = tableDef.schema;
        duckdb_error_data err = duckdb_schema_from_arrow(dbConn, &schemaCopy, &converted);
        if (err) {
          std::cerr << "Warning: " << entry.path().filename()
                    << " failed to convert schema for " << tableDef.table_name
                    << ": " << duckdb_error_data_message(err) << "\n";
          duckdb_destroy_error_data(&err);
          tablesOk = false;
          break;
        }

        plugin.tableMeta.push_back({tableDef.table_name, converted});
      }

      if (!tablesOk) {
        // Clean up any converted schemas we created before the failure
        for (auto& tm : plugin.tableMeta) {
          duckdb_destroy_arrow_converted_schema(&tm.convertedSchema);
        }
        continue;
      }

      std::cerr << "Loaded plugin: " << plugin.name
                << " v" << plugin.version << "\n";

      Plugins.push_back(std::move(plugin));
    }
    catch (const std::exception& e) {
      std::cerr << "Warning: failed to load " << entry.path().filename()
                << ": " << e.what() << "\n";
    }
  }
}

std::vector<PluginTableMeta> PluginManager::allTableMeta() const {
  std::vector<PluginTableMeta> all;
  for (const auto& plugin : Plugins) {
    all.insert(all.end(), plugin.tableMeta.begin(), plugin.tableMeta.end());
  }
  return all;
}

void PluginManager::shutdown() {
  for (auto it = Plugins.rbegin(); it != Plugins.rend(); ++it) {
    try {
      it->shutdown();
    }
    catch (...) {
      // Plugin shutdown errors are non-fatal
    }
    for (auto& tm : it->tableMeta) {
      duckdb_destroy_arrow_converted_schema(&tm.convertedSchema);
    }
  }
  Plugins.clear();
}
