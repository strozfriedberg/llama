#include <catch2/catch_test_macros.hpp>

#include "pluginmanager.h"
#include "llamaduck.h"

#include <filesystem>

TEST_CASE("testPluginManagerLoadNoDir") {
  LlamaDB db;
  LlamaDBConnection conn(db);
  PluginManager mgr;
  REQUIRE(mgr.pluginCount() == 0);
}

TEST_CASE("testPluginManagerLoadEmptyDir") {
  auto dir = std::filesystem::temp_directory_path() / "llama_test_empty_plugins";
  std::filesystem::create_directories(dir);
  LlamaDB db;
  LlamaDBConnection conn(db);
  PluginManager mgr;
  mgr.loadPlugins(dir, conn.get());
  REQUIRE(mgr.pluginCount() == 0);
  std::filesystem::remove_all(dir);
}

TEST_CASE("testPluginManagerLoadStub") {
  // The test stub plugin is built into the test build directory.
  // Find it relative to the test executable.
  auto pluginDir = std::filesystem::path(PLUGIN_STUB_DIR);
  REQUIRE(std::filesystem::exists(pluginDir));

  LlamaDB db;
  LlamaDBConnection conn(db);
  PluginManager mgr;
  mgr.loadPlugins(pluginDir, conn.get());
  REQUIRE(mgr.pluginCount() == 1);
  mgr.shutdown();
}
