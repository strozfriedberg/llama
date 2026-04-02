#include <catch2/catch_test_macros.hpp>

#include "pluginmanager.h"
#include "llamaduck.h"
#include "readseek_impl.h"
#include "plugin_api.h"
#include "processor.h"
#include "entry.h"

#include <filesystem>

namespace {
  LlamaReadSeek wrapReadSeek(ReadSeek* rs) {
    LlamaReadSeek lrs;
    lrs.opaque = static_cast<void*>(rs);
    lrs.read = [](void* o, uint8_t* buf, size_t len) -> int64_t {
      return static_cast<int64_t>(static_cast<ReadSeek*>(o)->read(len, buf));
    };
    lrs.seek = [](void* o, size_t pos) -> int64_t {
      static_cast<ReadSeek*>(o)->seek(pos);
      return 0;
    };
    lrs.size = [](void* o) -> uint64_t {
      return static_cast<ReadSeek*>(o)->size();
    };
    return lrs;
  }
}

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

TEST_CASE("testPluginManagerProcessFile") {
  auto pluginDir = std::filesystem::path(PLUGIN_STUB_DIR);
  LlamaDB db;
  LlamaDBConnection conn(db);
  PluginManager mgr;
  mgr.loadPlugins(pluginDir, conn.get());
  REQUIRE(mgr.pluginCount() == 1);

  // Create a ReadSeek with some content
  std::string content = "SQLite format 3\x00 fake sqlite content here";
  ReadSeekBuf rs(content);

  LlamaFileContext ctx;
  ctx.file_signature = "SQLite Database";
  ctx.inode_addr = 42;
  ctx.file_size = content.size();
  ctx.readseek = wrapReadSeek(&rs);

  std::string errorPlugin, errorMessage;
  int rc = mgr.processFile(ctx, errorPlugin, errorMessage);
  REQUIRE(rc == 0);

  mgr.shutdown();
}

TEST_CASE("testPluginManagerProcessFileNoMatch") {
  auto pluginDir = std::filesystem::path(PLUGIN_STUB_DIR);
  LlamaDB db;
  LlamaDBConnection conn(db);
  PluginManager mgr;
  mgr.loadPlugins(pluginDir, conn.get());

  std::string content = "just some text";
  ReadSeekBuf rs(content);

  LlamaFileContext ctx;
  ctx.file_signature = "Plain Text";
  ctx.inode_addr = 99;
  ctx.file_size = content.size();
  ctx.readseek = wrapReadSeek(&rs);

  std::string errorPlugin, errorMessage;
  int rc = mgr.processFile(ctx, errorPlugin, errorMessage);
  REQUIRE(rc == 0);

  mgr.shutdown();
}

TEST_CASE("testProcessorWithPlugin") {
  auto pluginDir = std::filesystem::path(PLUGIN_STUB_DIR);

  LlamaDB db;
  LlamaDBConnection conn(db);

  // Create tables needed by Processor
  auto ruleEngine = std::make_shared<LlamaRuleEngine>();
  ruleEngine->createTables(conn);
  DBType<HashRec>::createTable(conn.get(), "hash");
  DBType<SearchHit>::createTable(conn.get(), "search_hits");
  DBType<ExceptionRecord>::createTable(conn.get(), "exception_log");
  DBType<FileSigResult>::createTable(conn.get(), "file_signatures");

  // Load plugins
  PluginManager plugins;
  plugins.loadPlugins(pluginDir, conn.get());
  REQUIRE(plugins.pluginCount() == 1);

  // Create ProcessorContext with no lightgrep, no rules, no sigs
  MagicsType noMagics;
  auto procCtx = std::make_shared<ProcessorContext>(
    &db, nullptr, ruleEngine, "", "", noMagics
  );
  procCtx->Plugins = &plugins;

  Processor proc(procCtx);

  // Create an Entry with ReadSeek content
  std::string content = "SQLite format 3 fake content";
  auto rs = std::make_unique<ReadSeekBuf>(content);
  Entry entry(42, std::move(rs));
  entry.FileSize = content.size();
  entry.getStream().open();

  proc.process(entry);
  proc.flush();

  // If we got here without crash/hang, the plugin dispatch worked.
  plugins.shutdown();
}
