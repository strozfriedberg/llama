#include <catch2/catch_test_macros.hpp>

#include "pluginmanager.h"
#include "llamaduck.h"
#include "readseek_impl.h"
#include "plugin_bridge.h"
#include "processor.h"
#include "entry.h"

#include <filesystem>

TEST_CASE("testPluginManagerLoadNoDir") {
  PluginManager mgr;
  REQUIRE(mgr.pluginCount() == 0);
  REQUIRE(mgr.empty());
}

TEST_CASE("testPluginManagerLoadEmptyDir") {
  auto dir = std::filesystem::temp_directory_path() / "llama_test_empty_plugins";
  std::filesystem::create_directories(dir);
  LlamaDB db;
  LlamaDBConnection conn(db);
  PluginManager mgr;
  mgr.loadPlugins(dir, conn.get());
  REQUIRE(mgr.pluginCount() == 0);
  REQUIRE(mgr.empty());
  std::filesystem::remove_all(dir);
}

TEST_CASE("testPluginManagerLoadStub") {
  auto pluginDir = std::filesystem::path(PLUGIN_STUB_DIR);
  REQUIRE(std::filesystem::exists(pluginDir));

  // Create a temp dir with only the good stub to isolate from error stub
  auto tempDir = std::filesystem::temp_directory_path() / "llama_test_load_stub";
  std::filesystem::create_directories(tempDir);
  std::filesystem::copy_file(
    std::filesystem::path(PLUGIN_STUB),
    tempDir / std::filesystem::path(PLUGIN_STUB).filename(),
    std::filesystem::copy_options::overwrite_existing
  );

  LlamaDB db;
  LlamaDBConnection conn(db);
  PluginManager mgr;
  mgr.loadPlugins(tempDir, conn.get());
  REQUIRE(mgr.pluginCount() == 1);

  std::filesystem::remove_all(tempDir);
}

TEST_CASE("testPluginManagerProcessFile") {
  auto tempDir = std::filesystem::temp_directory_path() / "llama_test_process";
  std::filesystem::create_directories(tempDir);
  std::filesystem::copy_file(
    std::filesystem::path(PLUGIN_STUB),
    tempDir / std::filesystem::path(PLUGIN_STUB).filename(),
    std::filesystem::copy_options::overwrite_existing
  );

  LlamaDB db;
  LlamaDBConnection conn(db);
  PluginManager mgr;
  mgr.loadPlugins(tempDir, conn.get());
  REQUIRE(mgr.pluginCount() == 1);

  std::string content = "SQLite format 3\x00 fake sqlite content here";
  ReadSeekBuf rs(content);

  LlamaFileContext ctx{};
  ctx.struct_size = sizeof(LlamaFileContext);
  ctx.file_signature = "SQLite Database";
  ctx.inode_addr = 42;
  ctx.file_size = content.size();
  ctx.sha256 = "abc123";
  ctx.path = "/evidence/test.db";
  ctx.readseek = wrapReadSeek(&rs);

  const char* errmsg = nullptr;
  for (const auto& plugin : mgr.plugins()) {
    int rc = plugin.process(&ctx, &errmsg);
    REQUIRE(rc == 0);
    REQUIRE(errmsg == nullptr);
  }

  std::filesystem::remove_all(tempDir);
}

TEST_CASE("testPluginManagerProcessFileNoMatch") {
  auto tempDir = std::filesystem::temp_directory_path() / "llama_test_nomatch";
  std::filesystem::create_directories(tempDir);
  std::filesystem::copy_file(
    std::filesystem::path(PLUGIN_STUB),
    tempDir / std::filesystem::path(PLUGIN_STUB).filename(),
    std::filesystem::copy_options::overwrite_existing
  );

  LlamaDB db;
  LlamaDBConnection conn(db);
  PluginManager mgr;
  mgr.loadPlugins(tempDir, conn.get());

  std::string content = "just some text";
  ReadSeekBuf rs(content);

  LlamaFileContext ctx{};
  ctx.struct_size = sizeof(LlamaFileContext);
  ctx.file_signature = "Plain Text";
  ctx.inode_addr = 99;
  ctx.file_size = content.size();
  ctx.sha256 = "def456";
  ctx.path = "/evidence/readme.txt";
  ctx.readseek = wrapReadSeek(&rs);

  const char* errmsg = nullptr;
  for (const auto& plugin : mgr.plugins()) {
    int rc = plugin.process(&ctx, &errmsg);
    REQUIRE(rc == 0);
  }

  std::filesystem::remove_all(tempDir);
}

TEST_CASE("testPluginErrorReporting") {
  // Load only the error stub
  auto tempDir = std::filesystem::temp_directory_path() / "llama_test_error";
  std::filesystem::create_directories(tempDir);
  std::filesystem::copy_file(
    std::filesystem::path(PLUGIN_ERROR_STUB),
    tempDir / std::filesystem::path(PLUGIN_ERROR_STUB).filename(),
    std::filesystem::copy_options::overwrite_existing
  );

  LlamaDB db;
  LlamaDBConnection conn(db);
  PluginManager mgr;
  mgr.loadPlugins(tempDir, conn.get());
  REQUIRE(mgr.pluginCount() == 1);

  std::string content = "test data";
  ReadSeekBuf rs(content);

  LlamaFileContext ctx{};
  ctx.struct_size = sizeof(LlamaFileContext);
  ctx.file_signature = nullptr;
  ctx.inode_addr = 1;
  ctx.file_size = content.size();
  ctx.sha256 = "aaa111";
  ctx.path = nullptr;
  ctx.readseek = wrapReadSeek(&rs);

  const char* errmsg = nullptr;
  const auto& plugin = mgr.plugins()[0];
  int rc = plugin.process(&ctx, &errmsg);
  REQUIRE(rc < 0);
  REQUIRE(errmsg != nullptr);
  REQUIRE(std::string(errmsg) == "deliberate test error");
  plugin.freeError(errmsg);

  std::filesystem::remove_all(tempDir);
}

TEST_CASE("testPluginContinuesAfterError") {
  // Load both error stub and good stub
  auto tempDir = std::filesystem::temp_directory_path() / "llama_test_continue";
  std::filesystem::create_directories(tempDir);
  std::filesystem::copy_file(
    std::filesystem::path(PLUGIN_ERROR_STUB),
    tempDir / std::filesystem::path(PLUGIN_ERROR_STUB).filename(),
    std::filesystem::copy_options::overwrite_existing
  );
  std::filesystem::copy_file(
    std::filesystem::path(PLUGIN_STUB),
    tempDir / std::filesystem::path(PLUGIN_STUB).filename(),
    std::filesystem::copy_options::overwrite_existing
  );

  LlamaDB db;
  LlamaDBConnection conn(db);
  PluginManager mgr;
  mgr.loadPlugins(tempDir, conn.get());
  REQUIRE(mgr.pluginCount() == 2);

  std::string content = "test data";
  ReadSeekBuf rs(content);

  LlamaFileContext ctx{};
  ctx.struct_size = sizeof(LlamaFileContext);
  ctx.file_signature = nullptr;
  ctx.inode_addr = 1;
  ctx.file_size = content.size();
  ctx.sha256 = "bbb222";
  ctx.path = nullptr;
  ctx.readseek = wrapReadSeek(&rs);

  // Both plugins should be called -- one fails, one succeeds
  int errorCount = 0;
  int successCount = 0;
  for (const auto& plugin : mgr.plugins()) {
    ctx.readseek.seek(ctx.readseek.opaque, 0);
    const char* errmsg = nullptr;
    int rc = plugin.process(&ctx, &errmsg);
    if (rc < 0) {
      REQUIRE(errmsg != nullptr);
      plugin.freeError(errmsg);
      ++errorCount;
    } else {
      ++successCount;
    }
  }
  REQUIRE(errorCount == 1);
  REQUIRE(successCount == 1);

  std::filesystem::remove_all(tempDir);
}

TEST_CASE("testProcessorWithPlugin") {
  auto tempDir = std::filesystem::temp_directory_path() / "llama_test_processor";
  std::filesystem::create_directories(tempDir);
  std::filesystem::copy_file(
    std::filesystem::path(PLUGIN_STUB),
    tempDir / std::filesystem::path(PLUGIN_STUB).filename(),
    std::filesystem::copy_options::overwrite_existing
  );

  LlamaDB db;
  LlamaDBConnection conn(db);

  auto ruleEngine = std::make_shared<LlamaRuleEngine>();
  ruleEngine->createTables(conn);
  DBType<HashRec>::createTable(conn.get(), "hash");
  DBType<SearchHit>::createTable(conn.get(), "search_hits");
  DBType<ExceptionRecord>::createTable(conn.get(), "exception_log");
  DBType<FileSigResult>::createTable(conn.get(), "file_signatures");

  PluginManager plugins;
  plugins.loadPlugins(tempDir, conn.get());
  REQUIRE(plugins.pluginCount() == 1);

  MagicsType noMagics;
  auto procCtx = std::make_shared<ProcessorContext>(
    &db, nullptr, ruleEngine, "", "", noMagics, plugins
  );

  Processor proc(procCtx);

  std::string content = "SQLite format 3 fake content";
  auto rs = std::make_unique<ReadSeekBuf>(content);
  Entry entry(42, std::move(rs));
  entry.FileSize = content.size();
  entry.getStream().open();

  proc.process(entry);
  proc.flush();

  std::filesystem::remove_all(tempDir);
}
