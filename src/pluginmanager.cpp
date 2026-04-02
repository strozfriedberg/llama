#include "pluginmanager.h"

#include <iostream>

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

void PluginManager::loadPlugins(const std::filesystem::path& pluginDir, duckdb_connection& dbConn) {
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
          !lib.has("llama_plugin_last_error") ||
          !lib.has("llama_plugin_shutdown")) {
        std::cerr << "Warning: " << entry.path().filename()
                  << " missing required symbols, skipping\n";
        continue;
      }

      auto initFn = lib.get<LlamaPluginInfo(void*)>("llama_plugin_init");
      auto processFn = lib.get<int(const LlamaFileContext*)>("llama_plugin_process");
      auto lastErrorFn = lib.get<const char*()>("llama_plugin_last_error");
      auto shutdownFn = lib.get<void()>("llama_plugin_shutdown");

      LlamaPluginInfo info = initFn(&dbConn);
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
      plugin.lastError = lastErrorFn;
      plugin.shutdown = shutdownFn;

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

int PluginManager::processFile(const LlamaFileContext& ctx,
                                std::string& errorPlugin,
                                std::string& errorMessage) {
  for (auto& plugin : Plugins) {
    int rc = plugin.process(&ctx);
    if (rc < 0) {
      errorPlugin = plugin.name;
      const char* err = plugin.lastError();
      errorMessage = err ? err : "unknown error";
      return rc;
    }
  }
  return 0;
}

void PluginManager::shutdown() {
  for (auto it = Plugins.rbegin(); it != Plugins.rend(); ++it) {
    try {
      it->shutdown();
    }
    catch (...) {
      // Plugin shutdown errors are non-fatal
    }
  }
  Plugins.clear();
}
