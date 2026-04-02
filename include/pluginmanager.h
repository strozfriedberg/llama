#pragma once

#include "boost_dll.h"
#include "plugin_api.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <duckdb.h>

struct LoadedPlugin {
  std::string name;
  std::string version;
  boost::dll::shared_library library;
  std::function<int(const LlamaFileContext*)>  process;
  std::function<const char*()>                 lastError;
  std::function<void()>                        shutdown;
};

class PluginManager {
public:
  PluginManager() = default;
  ~PluginManager();

  PluginManager(const PluginManager&) = delete;
  PluginManager& operator=(const PluginManager&) = delete;

  void loadPlugins(const std::filesystem::path& pluginDir, duckdb_connection& dbConn);

  // Returns 0 if all plugins succeeded/skipped, negative on first error.
  // On error, errorPlugin and errorMessage are set.
  int processFile(const LlamaFileContext& ctx,
                  std::string& errorPlugin,
                  std::string& errorMessage);

  size_t pluginCount() const { return Plugins.size(); }

  void shutdown();

private:
  std::vector<LoadedPlugin> Plugins;

  static constexpr const char* pluginExtension();
};
