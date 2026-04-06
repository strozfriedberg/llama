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
  std::function<int(const LlamaFileContext*, const char**)> process;
  std::function<void(const char*)>                         freeError;
  std::function<void()>                                    shutdown;
};

class PluginManager {
public:
  // Thread-safety contract:
  //   - loadPlugins() and shutdown() are single-threaded (main thread).
  //   - plugins(), empty(), pluginCount() are const and safe to call
  //     concurrently from processing threads.
  //   - The Plugins vector is never mutated while processing threads are active.
  //   Lifecycle: loadPlugins() -> [processing...] -> shutdown()

  PluginManager() = default;
  ~PluginManager();

  PluginManager(const PluginManager&) = delete;
  PluginManager& operator=(const PluginManager&) = delete;

  void loadPlugins(const std::filesystem::path& pluginDir, duckdb_connection dbConn);

  const std::vector<LoadedPlugin>& plugins() const { return Plugins; }
  bool empty() const { return Plugins.empty(); }
  size_t pluginCount() const { return Plugins.size(); }

  void shutdown();

private:
  std::vector<LoadedPlugin> Plugins;

  static constexpr const char* pluginExtension();
};
