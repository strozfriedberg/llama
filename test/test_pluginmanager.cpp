#include <catch2/catch_test_macros.hpp>

#include "pluginmanager.h"
#include "plugin_table_writer.h"
#include "llamaduck.h"
#include "readseek_impl.h"
#include "plugin_bridge.h"
#include "processor.h"
#include "entry.h"
#include "arrow/c/abi.h"

#include <filesystem>

static void noop_schema_release(struct ArrowSchema*) {}
static void noop_array_release(struct ArrowArray* a) { a->release = nullptr; }

namespace {
  LlamaWriteContext dummyWriteCtx() {
    LlamaWriteContext ctx{};
    ctx.opaque = nullptr;
    ctx.write = nullptr;
    return ctx;
  }
}

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
    auto writeCtx = dummyWriteCtx();
    int rc = plugin.process(&ctx, &writeCtx, &errmsg);
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
    auto writeCtx = dummyWriteCtx();
    int rc = plugin.process(&ctx, &writeCtx, &errmsg);
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
  auto writeCtx = dummyWriteCtx();
  int rc = plugin.process(&ctx, &writeCtx, &errmsg);
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
    auto writeCtx = dummyWriteCtx();
    int rc = plugin.process(&ctx, &writeCtx, &errmsg);
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

TEST_CASE("testPluginManagerCreatesTable") {
  auto tempDir = std::filesystem::temp_directory_path() / "llama_test_table_creation";
  std::filesystem::create_directories(tempDir);
  std::filesystem::copy_file(
    std::filesystem::path(PLUGIN_TABLE_STUB),
    tempDir / std::filesystem::path(PLUGIN_TABLE_STUB).filename(),
    std::filesystem::copy_options::overwrite_existing
  );

  LlamaDB db;
  LlamaDBConnection conn(db);
  PluginManager mgr;
  mgr.loadPlugins(tempDir, conn.get());
  REQUIRE(mgr.pluginCount() == 1);

  // Verify the table was created in DuckDB
  duckdb_result result;
  auto rc = duckdb_query(conn.get(), "SELECT * FROM plugin_test_data", &result);
  REQUIRE(rc == DuckDBSuccess);
  REQUIRE(duckdb_column_count(&result) == 2);
  REQUIRE(std::string(duckdb_column_name(&result, 0)) == "name");
  REQUIRE(std::string(duckdb_column_name(&result, 1)) == "value");
  REQUIRE(duckdb_row_count(&result) == 0);  // empty table
  duckdb_destroy_result(&result);

  // Verify table metadata is accessible
  auto meta = mgr.allTableMeta();
  REQUIRE(meta.size() == 1);
  REQUIRE(meta[0].tableName == "plugin_test_data");
  REQUIRE(meta[0].convertedSchema != nullptr);

  std::filesystem::remove_all(tempDir);
}

TEST_CASE("testPluginTableWriterArrowBatch") {
  LlamaDB db;
  LlamaDBConnection conn(db);
  duckdb_query(conn.get(), "CREATE TABLE plugin_test_data (name VARCHAR, value UBIGINT);", nullptr);

  // Build Arrow schema for the table (matching what PluginManager would cache)
  struct ArrowSchema name_schema{};
  name_schema.format = "u";
  name_schema.name = "name";
  name_schema.n_children = 0;
  name_schema.children = nullptr;
  name_schema.release = noop_schema_release;

  struct ArrowSchema value_schema{};
  value_schema.format = "L";
  value_schema.name = "value";
  value_schema.n_children = 0;
  value_schema.children = nullptr;
  value_schema.release = noop_schema_release;

  struct ArrowSchema* schema_children[] = {&name_schema, &value_schema};

  struct ArrowSchema batch_schema{};
  batch_schema.format = "+s";
  batch_schema.name = "";
  batch_schema.n_children = 2;
  batch_schema.children = schema_children;
  batch_schema.release = noop_schema_release;

  // Convert schema (as PluginManager does)
  duckdb_arrow_converted_schema converted = nullptr;
  duckdb_error_data err = duckdb_schema_from_arrow(conn.get(), &batch_schema, &converted);
  REQUIRE(!err);

  std::vector<PluginTableMeta> meta = {{"plugin_test_data", converted}};
  PluginTableWriter writer(&db, meta);

  // Build a one-row Arrow batch: name="hello", value=42
  int32_t offsets[] = {0, 5};
  const char str_data[] = "hello";
  const void* string_buffers[] = {nullptr, offsets, str_data};

  struct ArrowArray string_col{};
  string_col.length = 1;
  string_col.null_count = 0;
  string_col.offset = 0;
  string_col.n_buffers = 3;
  string_col.buffers = string_buffers;
  string_col.n_children = 0;
  string_col.children = nullptr;
  string_col.release = noop_array_release;

  uint64_t values[] = {42};
  const void* uint_buffers[] = {nullptr, values};

  struct ArrowArray uint_col{};
  uint_col.length = 1;
  uint_col.null_count = 0;
  uint_col.offset = 0;
  uint_col.n_buffers = 2;
  uint_col.buffers = uint_buffers;
  uint_col.n_children = 0;
  uint_col.children = nullptr;
  uint_col.release = noop_array_release;

  struct ArrowArray* batch_children[] = {&string_col, &uint_col};
  const void* struct_buffers[] = {nullptr};

  struct ArrowArray batch{};
  batch.length = 1;
  batch.null_count = 0;
  batch.offset = 0;
  batch.n_buffers = 1;
  batch.buffers = struct_buffers;
  batch.n_children = 2;
  batch.children = batch_children;
  batch.release = noop_array_release;

  // Call write
  int rc = writer.write("plugin_test_data", &batch_schema, &batch);
  REQUIRE(rc == 0);

  writer.close();

  // Verify data in DuckDB
  duckdb_result result;
  REQUIRE(duckdb_query(conn.get(), "SELECT name, value FROM plugin_test_data", &result) == DuckDBSuccess);
  REQUIRE(duckdb_row_count(&result) == 1);

  auto name_val = duckdb_value_varchar(&result, 0, 0);
  REQUIRE(std::string(name_val) == "hello");
  duckdb_free(name_val);

  auto num_val = duckdb_value_uint64(&result, 1, 0);
  REQUIRE(num_val == 42);

  duckdb_destroy_result(&result);
  duckdb_destroy_arrow_converted_schema(&converted);
}

TEST_CASE("testPluginTableWriterUnknownTable") {
  LlamaDB db;
  LlamaDBConnection conn(db);
  duckdb_query(conn.get(), "CREATE TABLE plugin_test_data (name VARCHAR, value UBIGINT);", nullptr);

  struct ArrowSchema name_schema{};
  name_schema.format = "u";
  name_schema.name = "name";
  name_schema.n_children = 0;
  name_schema.release = noop_schema_release;

  struct ArrowSchema value_schema{};
  value_schema.format = "L";
  value_schema.name = "value";
  value_schema.n_children = 0;
  value_schema.release = noop_schema_release;

  struct ArrowSchema* schema_children[] = {&name_schema, &value_schema};

  struct ArrowSchema batch_schema{};
  batch_schema.format = "+s";
  batch_schema.name = "";
  batch_schema.n_children = 2;
  batch_schema.children = schema_children;
  batch_schema.release = noop_schema_release;

  duckdb_arrow_converted_schema converted = nullptr;
  duckdb_error_data e = duckdb_schema_from_arrow(conn.get(), &batch_schema, &converted);
  REQUIRE(!e);

  std::vector<PluginTableMeta> meta = {{"plugin_test_data", converted}};
  PluginTableWriter writer(&db, meta);

  struct ArrowArray dummy{};
  int rc = writer.write("nonexistent_table", &batch_schema, &dummy);
  REQUIRE(rc < 0);

  writer.close();
  duckdb_destroy_arrow_converted_schema(&converted);
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
