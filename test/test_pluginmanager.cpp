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
