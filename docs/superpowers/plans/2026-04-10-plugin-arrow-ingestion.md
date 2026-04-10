# Plugin Arrow Ingestion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the DuckDB connection passthrough in the plugin API with Arrow C Data Interface callbacks, so plugins produce Arrow record batches and llama handles all DuckDB operations.

**Architecture:** Update the C ABI header with Arrow-based table declarations (init) and write callbacks (process). PluginManager converts Arrow schemas to DuckDB tables at init, caches converted schemas. A new PluginTableWriter type (per-Processor) implements the write callback by converting Arrow batches to DuckDB data chunks and appending them.

**Tech Stack:** C/C++20, DuckDB C API (Arrow ingestion functions), Apache Arrow C Data Interface (vendored header-only)

**Spec:** `docs/superpowers/specs/2026-04-10-plugin-arrow-ingestion-design.md`

---

## File Map

**Create:**
- `include/arrow/c/abi.h` — vendored Arrow C Data Interface header (frozen ABI, ~40 lines)
- `include/plugin_table_writer.h` — PluginTableWriter: per-Processor Arrow→DuckDB write path
- `src/plugin_table_writer.cpp` — PluginTableWriter implementation
- `test/test_plugin_table_stub.c` — test stub that declares a table via the new init protocol

**Modify:**
- `include/plugin_api.h` — new types (LlamaTableDef, LlamaWriteArrow, LlamaWriteContext), updated init/process signatures
- `include/pluginmanager.h` — LoadedPlugin gains table metadata; PluginManager exposes it
- `src/pluginmanager.cpp` — new init protocol, Arrow schema→DuckDB table creation, schema caching
- `include/processor.h` — add PluginTableWriter member
- `src/processor.cpp` — construct write context in dispatch loop, close writer in flush
- `test/test_plugin_stub.c` — update for new init/process signatures
- `test/test_plugin_error_stub.c` — update for new init/process signatures
- `test/test_pluginmanager.cpp` — update call sites, add table creation and Arrow write tests
- `src/meson.build` — add plugin_table_writer.cpp
- `test/meson.build` — add table stub, pass path define
- `Makefile.am` — add new source files

---

### Task 1: Vendor Arrow C Data Interface Header and Update Plugin API

This task updates the C ABI header with the new Arrow-based types and
signatures. No behavior changes yet — just type definitions.

**Files:**
- Create: `include/arrow/c/abi.h`
- Modify: `include/plugin_api.h`

- [ ] **Step 1: Create vendored Arrow C Data Interface header**

Create `include/arrow/c/abi.h`:

```c
/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information regarding
 * copyright ownership.  The ASF licenses this file to you under
 * the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Vendored from the Apache Arrow C Data Interface specification:
 * https://arrow.apache.org/docs/format/CDataInterface.html
 *
 * This header defines a frozen ABI — the struct layouts will never change.
 * If upstream revises the specification, update this file from the link above.
 */

#ifndef ARROW_C_DATA_INTERFACE
#define ARROW_C_DATA_INTERFACE

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ArrowSchema {
  const char* format;
  const char* name;
  const char* metadata;
  int64_t flags;
  int64_t n_children;
  struct ArrowSchema** children;
  struct ArrowSchema* dictionary;
  void (*release)(struct ArrowSchema*);
  void* private_data;
};

struct ArrowArray {
  int64_t length;
  int64_t null_count;
  int64_t offset;
  int64_t n_buffers;
  int64_t n_children;
  const void** buffers;
  struct ArrowArray** children;
  struct ArrowArray* dictionary;
  void (*release)(struct ArrowArray*);
  void* private_data;
};

#ifdef __cplusplus
}
#endif

#endif /* ARROW_C_DATA_INTERFACE */
```

- [ ] **Step 2: Update plugin_api.h with new types and signatures**

Replace the full contents of `include/plugin_api.h`:

```c
#ifndef LLAMA_PLUGIN_API_H
#define LLAMA_PLUGIN_API_H

#include <stdint.h>
#include <stddef.h>

#include "arrow/c/abi.h"

#ifdef _WIN32
  #define LLAMA_PLUGIN_EXPORT __declspec(dllexport)
#else
  #define LLAMA_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void*     opaque;
    /* Note: parameter order is (buf, len), matching Rust's Read::read convention.
       C++ ReadSeek::read uses (len, buf) -- the bridge handles the transposition. */
    int64_t   (*read)(void* opaque, uint8_t* buf, size_t len);
    int64_t   (*seek)(void* opaque, size_t pos);   /* returns new position, or negative on error */
    uint64_t  (*size)(void* opaque);
} LlamaReadSeek;

typedef struct {
    uint32_t      struct_size;     /* sizeof(LlamaFileContext) */
    const char*   file_signature;  /* NULL if no signature matched */
    uint64_t      inode_addr;
    uint64_t      file_size;
    const char*   sha256;          /* hex-encoded SHA-256 hash, always set */
    const char*   path;            /* full path including filename, or NULL */
    LlamaReadSeek readseek;
} LlamaFileContext;

typedef struct {
    uint32_t    struct_size;       /* sizeof(LlamaPluginInfo) */
    const char* name;
    const char* version;
} LlamaPluginInfo;

/* A (table_name, schema) pair declared by the plugin during init.
   The ArrowSchema should have release = NULL to indicate static lifetime. */
typedef struct {
    const char*        table_name;
    struct ArrowSchema schema;
} LlamaTableDef;

/* Callback for the plugin to flush Arrow record batches to llama.
   table_name:  which table to write to (must match a declared table).
   schema:      Arrow schema for this batch.
   array:       Arrow array (record batch) to append.
   Returns 0 on success, negative on failure. */
typedef int (*LlamaWriteArrow)(
    void*                opaque,
    const char*          table_name,
    struct ArrowSchema*  schema,
    struct ArrowArray*   array
);

/* Context passed to llama_plugin_process alongside the file context. */
typedef struct {
    void*            opaque;
    LlamaWriteArrow  write;
} LlamaWriteContext;

/* Plugin lifecycle.
   Fills in info with plugin metadata.
   Sets *tables to point to an array of table definitions and *num_tables
   to the count. The array and its ArrowSchema contents must have static
   lifetime (plugin owns the memory, llama borrows it).
   Returns 0 on success, negative on failure. */
LLAMA_PLUGIN_EXPORT int  llama_plugin_init(
    LlamaPluginInfo*      info,
    const LlamaTableDef** tables,
    size_t*               num_tables
);
LLAMA_PLUGIN_EXPORT void llama_plugin_shutdown(void);

/* Per-file processing -- returns 0 on success/skip, negative on error.
   On error, *errmsg is set to a plugin-allocated error string.
   Caller must pass *errmsg to llama_plugin_free_error() when done.
   Called from multiple threads concurrently -- plugins must be thread-safe. */
LLAMA_PLUGIN_EXPORT int  llama_plugin_process(
    const LlamaFileContext*  ctx,
    const LlamaWriteContext* write_ctx,
    const char**             errmsg
);

/* Free an error string returned via llama_plugin_process. */
LLAMA_PLUGIN_EXPORT void llama_plugin_free_error(const char* errmsg);

#ifdef __cplusplus
}
#endif

#endif /* LLAMA_PLUGIN_API_H */
```

- [ ] **Step 3: Verify headers compile**

Run: `echo '#include "plugin_api.h"' | c++ -std=c++20 -fsyntax-only -I include -x c++ -`

Expected: no errors.

- [ ] **Step 4: Commit**

```bash
git add include/arrow/c/abi.h include/plugin_api.h
git commit -m "feat(plugin): vendor Arrow C ABI header and update plugin_api.h

Add Arrow C Data Interface types (LlamaTableDef, LlamaWriteArrow,
LlamaWriteContext) and update init/process signatures to use
Arrow-based table declarations and write callbacks."
```

---

### Task 2: Update Test Stubs for New Signatures

Update both C test stubs to match the new init/process function signatures.
They declare zero tables and ignore the write context.

**Files:**
- Modify: `test/test_plugin_stub.c`
- Modify: `test/test_plugin_error_stub.c`

- [ ] **Step 1: Update test_plugin_stub.c**

Replace the full contents of `test/test_plugin_stub.c`:

```c
#include "plugin_api.h"
#include <string.h>
#include <stdlib.h>

LLAMA_PLUGIN_EXPORT int llama_plugin_init(LlamaPluginInfo* info,
                                          const LlamaTableDef** tables,
                                          size_t* num_tables) {
    info->struct_size = sizeof(LlamaPluginInfo);
    info->name = "test-plugin";
    info->version = "0.1.0";
    *tables = NULL;
    *num_tables = 0;
    return 0;
}

LLAMA_PLUGIN_EXPORT int llama_plugin_process(const LlamaFileContext* ctx,
                                             const LlamaWriteContext* write_ctx,
                                             const char** errmsg) {
    (void)write_ctx;
    if (ctx->file_signature && strcmp(ctx->file_signature, "SQLite Database") == 0) {
        /* Read first 16 bytes to verify ReadSeek works */
        uint8_t buf[16];
        int64_t n = ctx->readseek.read(ctx->readseek.opaque, buf, 16);
        if (n < 0) {
            *errmsg = strdup("read failed");
            return -1;
        }
        return 0;
    }
    return 0;
}

LLAMA_PLUGIN_EXPORT void llama_plugin_free_error(const char* errmsg) {
    free((void*)errmsg);
}

LLAMA_PLUGIN_EXPORT void llama_plugin_shutdown(void) {
}
```

- [ ] **Step 2: Update test_plugin_error_stub.c**

Replace the full contents of `test/test_plugin_error_stub.c`:

```c
#include "plugin_api.h"
#include <stdlib.h>
#include <string.h>

LLAMA_PLUGIN_EXPORT int llama_plugin_init(LlamaPluginInfo* info,
                                          const LlamaTableDef** tables,
                                          size_t* num_tables) {
    info->struct_size = sizeof(LlamaPluginInfo);
    info->name = "error-plugin";
    info->version = "0.1.0";
    *tables = NULL;
    *num_tables = 0;
    return 0;
}

LLAMA_PLUGIN_EXPORT int llama_plugin_process(const LlamaFileContext* ctx,
                                             const LlamaWriteContext* write_ctx,
                                             const char** errmsg) {
    (void)ctx;
    (void)write_ctx;
    *errmsg = strdup("deliberate test error");
    return -1;
}

LLAMA_PLUGIN_EXPORT void llama_plugin_free_error(const char* errmsg) {
    free((void*)errmsg);
}

LLAMA_PLUGIN_EXPORT void llama_plugin_shutdown(void) {
}
```

- [ ] **Step 3: Verify stubs compile**

Run: `meson compile -C builddir test_plugin_stub test_plugin_error_stub`

Expected: both shared libraries build successfully.

- [ ] **Step 4: Commit**

```bash
git add test/test_plugin_stub.c test/test_plugin_error_stub.c
git commit -m "test(plugin): update stubs for Arrow-based plugin API signatures"
```

---

### Task 3: Update PluginManager and Processor for New Signatures

Update the C++ side to match the new C ABI. PluginManager calls the new
init (ignoring tables for now). Processor passes an empty write context.
All existing tests should pass afterward.

**Files:**
- Modify: `include/pluginmanager.h`
- Modify: `src/pluginmanager.cpp`
- Modify: `src/processor.cpp`
- Modify: `test/test_pluginmanager.cpp`

- [ ] **Step 1: Update pluginmanager.h**

In `include/pluginmanager.h`, update `LoadedPlugin` and `PluginManager`:

Replace:
```cpp
struct LoadedPlugin {
  std::string name;
  std::string version;
  boost::dll::shared_library library;
  std::function<int(const LlamaFileContext*, const char**)> process;
  std::function<void(const char*)>                         freeError;
  std::function<void()>                                    shutdown;
};
```

With:
```cpp
struct PluginTableMeta {
  std::string tableName;
  duckdb_arrow_converted_schema convertedSchema;
};

struct LoadedPlugin {
  std::string name;
  std::string version;
  boost::dll::shared_library library;
  std::function<int(const LlamaFileContext*, const LlamaWriteContext*, const char**)> process;
  std::function<void(const char*)>                         freeError;
  std::function<void()>                                    shutdown;
  std::vector<PluginTableMeta> tableMeta;
};
```

Add a public accessor to `PluginManager`:

```cpp
  // All table metadata across all plugins (for constructing PluginTableWriters)
  std::vector<PluginTableMeta> allTableMeta() const;
```

- [ ] **Step 2: Update pluginmanager.cpp**

In `src/pluginmanager.cpp`, update `loadPlugins()`:

Change the `initFn` and `processFn` type resolution:

Replace:
```cpp
      auto initFn    = lib.get<int(void*, LlamaPluginInfo*)>("llama_plugin_init");
      auto processFn = lib.get<int(const LlamaFileContext*, const char**)>("llama_plugin_process");
```

With:
```cpp
      auto initFn    = lib.get<int(LlamaPluginInfo*, const LlamaTableDef**, size_t*)>("llama_plugin_init");
      auto processFn = lib.get<int(const LlamaFileContext*, const LlamaWriteContext*, const char**)>("llama_plugin_process");
```

Change the init call:

Replace:
```cpp
      LlamaPluginInfo info{};
      if (initFn(dbConn, &info) < 0) {
```

With:
```cpp
      LlamaPluginInfo info{};
      const LlamaTableDef* tables = nullptr;
      size_t numTables = 0;
      if (initFn(&info, &tables, &numTables) < 0) {
```

After the plugin is created and before `Plugins.push_back`, add table handling
(table creation and schema caching is implemented in Task 5 — for now just
log the table count):

```cpp
      if (numTables > 0) {
        std::cerr << "  Plugin declares " << numTables << " tables (creation pending)\n";
      }
```

Add the `allTableMeta()` implementation at the bottom of the file:

```cpp
std::vector<PluginTableMeta> PluginManager::allTableMeta() const {
  std::vector<PluginTableMeta> all;
  for (const auto& plugin : Plugins) {
    all.insert(all.end(), plugin.tableMeta.begin(), plugin.tableMeta.end());
  }
  return all;
}
```

- [ ] **Step 3: Update processor.cpp dispatch loop**

In `src/processor.cpp`, update the plugin dispatch (around line 180):

Replace:
```cpp
    for (const auto& plugin : Context->Plugins.plugins()) {
      pluginCtx.readseek.seek(pluginCtx.readseek.opaque, 0);
      const char* errmsg = nullptr;
      int rc;
      try {
        rc = plugin.process(&pluginCtx, &errmsg);
```

With:
```cpp
    LlamaWriteContext writeCtx{};
    writeCtx.opaque = nullptr;
    writeCtx.write = nullptr;

    for (const auto& plugin : Context->Plugins.plugins()) {
      pluginCtx.readseek.seek(pluginCtx.readseek.opaque, 0);
      const char* errmsg = nullptr;
      int rc;
      try {
        rc = plugin.process(&pluginCtx, &writeCtx, &errmsg);
```

- [ ] **Step 4: Update test_pluginmanager.cpp**

Every `plugin.process(&ctx, &errmsg)` call in `test/test_pluginmanager.cpp`
needs an added `LlamaWriteContext` parameter.

Add a helper at the top of the test file (after includes):

```cpp
namespace {
  LlamaWriteContext dummyWriteCtx() {
    LlamaWriteContext ctx{};
    ctx.opaque = nullptr;
    ctx.write = nullptr;
    return ctx;
  }
}
```

Then in each test, before the `plugin.process` call, add:

```cpp
  auto writeCtx = dummyWriteCtx();
```

And change every:
```cpp
    int rc = plugin.process(&ctx, &errmsg);
```
To:
```cpp
    int rc = plugin.process(&ctx, &writeCtx, &errmsg);
```

This applies to: `testPluginManagerProcessFile`, `testPluginManagerProcessFileNoMatch`,
`testPluginErrorReporting`, `testPluginContinuesAfterError`.

The `testProcessorWithPlugin` test calls `proc.process(entry)` which goes through
Processor — no change needed there (Processor constructs the write context internally).

- [ ] **Step 5: Build and run all plugin tests**

Run:
```bash
meson compile -C builddir && builddir/test/llama_test "testPlugin*" "testProcessor*"
```

Expected: all 10 plugin/processor tests pass.

- [ ] **Step 6: Commit**

```bash
git add include/pluginmanager.h src/pluginmanager.cpp src/processor.cpp test/test_pluginmanager.cpp
git commit -m "refactor(plugin): update PluginManager and Processor for Arrow-based API

PluginManager calls new init signature (no DuckDB handle, returns table
defs). Processor passes write context to plugin process. Table creation
from Arrow schemas is a no-op pending next task."
```

---

### Task 4: Create Table-Declaring Test Stub

A C test stub that declares a table with two columns (name VARCHAR,
value UINT64) via the new init protocol. Used by tests in Tasks 5 and 6.

**Files:**
- Create: `test/test_plugin_table_stub.c`
- Modify: `test/meson.build`

- [ ] **Step 1: Create test_plugin_table_stub.c**

Create `test/test_plugin_table_stub.c`:

```c
#include "plugin_api.h"
#include <stdlib.h>
#include <string.h>

/* Static schema for a table with two columns: name (utf8), value (uint64) */
static struct ArrowSchema child_schemas[2];
static struct ArrowSchema* children[2] = { &child_schemas[0], &child_schemas[1] };
static struct ArrowSchema table_schema;
static LlamaTableDef table_defs[1];

LLAMA_PLUGIN_EXPORT int llama_plugin_init(LlamaPluginInfo* info,
                                          const LlamaTableDef** tables,
                                          size_t* num_tables) {
    info->struct_size = sizeof(LlamaPluginInfo);
    info->name = "table-plugin";
    info->version = "0.1.0";

    /* Column 0: name (utf8 string) */
    memset(&child_schemas[0], 0, sizeof(struct ArrowSchema));
    child_schemas[0].format = "u";
    child_schemas[0].name = "name";
    child_schemas[0].n_children = 0;
    child_schemas[0].children = NULL;
    child_schemas[0].release = NULL;

    /* Column 1: value (uint64) */
    memset(&child_schemas[1], 0, sizeof(struct ArrowSchema));
    child_schemas[1].format = "L";
    child_schemas[1].name = "value";
    child_schemas[1].n_children = 0;
    child_schemas[1].children = NULL;
    child_schemas[1].release = NULL;

    /* Top-level struct schema */
    memset(&table_schema, 0, sizeof(struct ArrowSchema));
    table_schema.format = "+s";
    table_schema.name = "";
    table_schema.n_children = 2;
    table_schema.children = children;
    table_schema.release = NULL;

    table_defs[0].table_name = "plugin_test_data";
    table_defs[0].schema = table_schema;

    *tables = table_defs;
    *num_tables = 1;
    return 0;
}

LLAMA_PLUGIN_EXPORT int llama_plugin_process(const LlamaFileContext* ctx,
                                             const LlamaWriteContext* write_ctx,
                                             const char** errmsg) {
    (void)ctx;
    (void)write_ctx;
    (void)errmsg;
    return 0; /* always skip */
}

LLAMA_PLUGIN_EXPORT void llama_plugin_free_error(const char* errmsg) {
    free((void*)errmsg);
}

LLAMA_PLUGIN_EXPORT void llama_plugin_shutdown(void) {
}
```

- [ ] **Step 2: Add table stub to test/meson.build**

In `test/meson.build`, after the `test_plugin_error_stub` block, add:

```meson
test_plugin_table_stub = shared_library('test_plugin_table_stub',
  'test_plugin_table_stub.c',
  include_directories: inc,
  name_prefix: '',
  c_args: ['-std=c11'],
  build_by_default: true,
)
```

In the `test_exe` cpp_args, add:

```meson
      '-DPLUGIN_TABLE_STUB="' + test_plugin_table_stub.full_path() + '"',
```

- [ ] **Step 3: Build**

Run: `meson compile -C builddir`

Expected: `test_plugin_table_stub` shared library builds.

- [ ] **Step 4: Commit**

```bash
git add test/test_plugin_table_stub.c test/meson.build
git commit -m "test(plugin): add table-declaring stub for Arrow ingestion tests"
```

---

### Task 5: Implement PluginManager Table Creation from Arrow Schemas

PluginManager converts Arrow schemas to DuckDB tables at init time and
caches the converted schemas for later use by PluginTableWriter.

**Files:**
- Modify: `include/pluginmanager.h`
- Modify: `src/pluginmanager.cpp`
- Modify: `test/test_pluginmanager.cpp`

- [ ] **Step 1: Write failing test**

Add to `test/test_pluginmanager.cpp`:

```cpp
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
```

- [ ] **Step 2: Run test to verify it fails**

Run: `meson compile -C builddir && builddir/test/llama_test "testPluginManagerCreatesTable"`

Expected: FAIL — table does not exist (PluginManager doesn't create tables yet).

- [ ] **Step 3: Add Arrow format→DuckDB type mapping to pluginmanager.cpp**

Add at the top of `src/pluginmanager.cpp` (inside an anonymous namespace):

```cpp
#include <cstring>

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

std::string buildCreateTableSQL(const char* tableName, const struct ArrowSchema& schema) {
  std::string sql = "CREATE TABLE ";
  sql += tableName;
  sql += " (";
  for (int64_t c = 0; c < schema.n_children; ++c) {
    if (c > 0) sql += ", ";
    sql += schema.children[c]->name;
    sql += " ";
    const char* duckType = arrowFormatToDuckDBType(schema.children[c]->format);
    if (!duckType) return {};  // unsupported type
    sql += duckType;
  }
  sql += ");";
  return sql;
}

} // anonymous namespace
```

- [ ] **Step 4: Implement table creation and schema caching in loadPlugins**

In `src/pluginmanager.cpp`, in `loadPlugins()`, replace the temporary log line:

```cpp
      if (numTables > 0) {
        std::cerr << "  Plugin declares " << numTables << " tables (creation pending)\n";
      }
```

With the full table creation logic, placed just before `Plugins.push_back`:

```cpp
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
```

- [ ] **Step 5: Add converted schema cleanup to shutdown**

In `src/pluginmanager.cpp`, update `shutdown()`:

Replace:
```cpp
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
```

With:
```cpp
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
```

- [ ] **Step 6: Run test to verify it passes**

Run: `meson compile -C builddir && builddir/test/llama_test "testPluginManagerCreatesTable"`

Expected: PASS.

- [ ] **Step 7: Run all plugin tests to verify no regressions**

Run: `builddir/test/llama_test "testPlugin*" "testProcessor*"`

Expected: all pass.

- [ ] **Step 8: Commit**

```bash
git add include/pluginmanager.h src/pluginmanager.cpp test/test_pluginmanager.cpp
git commit -m "feat(plugin): create DuckDB tables from Arrow schemas at init

PluginManager now converts plugin-declared ArrowSchemas to DuckDB
tables and caches converted schemas for later use during Arrow
batch ingestion."
```

---

### Task 6: Implement PluginTableWriter

The core new type. Receives Arrow record batches via a write callback,
converts them to DuckDB data chunks, and appends them.

**Files:**
- Create: `include/plugin_table_writer.h`
- Create: `src/plugin_table_writer.cpp`
- Modify: `test/test_pluginmanager.cpp` (add Arrow write path test)
- Modify: `src/meson.build` (add source)
- Modify: `test/meson.build` (add source to common list)

- [ ] **Step 1: Write failing test — Arrow batch through write callback lands in DuckDB**

Add to `test/test_pluginmanager.cpp`, after includes:

```cpp
#include "plugin_table_writer.h"
#include "arrow/c/abi.h"
```

Add test:

```cpp
TEST_CASE("testPluginTableWriterArrowBatch") {
  // Set up DuckDB with a table matching the table stub's schema
  LlamaDB db;
  LlamaDBConnection conn(db);
  duckdb_query(conn.get(), "CREATE TABLE plugin_test_data (name VARCHAR, value UBIGINT);", nullptr);

  // Convert the schema (same as what PluginManager does at init)
  struct ArrowSchema name_schema{};
  name_schema.format = "u";
  name_schema.name = "name";
  name_schema.n_children = 0;
  name_schema.release = nullptr;

  struct ArrowSchema value_schema{};
  value_schema.format = "L";
  value_schema.name = "value";
  value_schema.n_children = 0;
  value_schema.release = nullptr;

  struct ArrowSchema* schema_children[] = {&name_schema, &value_schema};

  struct ArrowSchema batch_schema{};
  batch_schema.format = "+s";
  batch_schema.name = "";
  batch_schema.n_children = 2;
  batch_schema.children = schema_children;
  batch_schema.release = nullptr;

  duckdb_arrow_converted_schema converted = nullptr;
  duckdb_error_data err = duckdb_schema_from_arrow(conn.get(), &batch_schema, &converted);
  REQUIRE(!err);

  // Build table metadata (normally provided by PluginManager)
  std::vector<PluginTableMeta> meta = {{"plugin_test_data", converted}};

  // Create the writer
  PluginTableWriter writer(&db, meta);

  // Build a one-row Arrow batch: name="hello", value=42
  // String column: offsets + data
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
  string_col.release = [](struct ArrowArray* a) { a->release = nullptr; };

  // Uint64 column
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
  uint_col.release = [](struct ArrowArray* a) { a->release = nullptr; };

  // Struct (record batch)
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
  batch.release = [](struct ArrowArray* a) { a->release = nullptr; };

  // Call the write method
  int rc = writer.write("plugin_test_data", &batch_schema, &batch);
  REQUIRE(rc == 0);

  // Close the writer (flushes appenders)
  writer.close();

  // Verify data landed in DuckDB
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
```

- [ ] **Step 2: Run test to verify it fails**

Run: `meson compile -C builddir && builddir/test/llama_test "testPluginTableWriterArrowBatch"`

Expected: FAIL — `plugin_table_writer.h` not found.

- [ ] **Step 3: Write plugin_table_writer.h**

Create `include/plugin_table_writer.h`:

```cpp
#pragma once

#include "llamaduck.h"
#include "pluginmanager.h"
#include "plugin_api.h"

#include <string>
#include <unordered_map>
#include <vector>

class PluginTableWriter {
public:
  // tableMeta is borrowed -- caller (PluginManager) must outlive the writer.
  PluginTableWriter(LlamaDB* db, const std::vector<PluginTableMeta>& tableMeta);
  ~PluginTableWriter();

  PluginTableWriter(const PluginTableWriter&) = delete;
  PluginTableWriter& operator=(const PluginTableWriter&) = delete;

  // Convert an Arrow batch and append it to the named table.
  // Returns 0 on success, negative on error.
  int write(const char* tableName, struct ArrowSchema* schema, struct ArrowArray* array);

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
};
```

- [ ] **Step 4: Write plugin_table_writer.cpp**

Create `src/plugin_table_writer.cpp`:

```cpp
#include "plugin_table_writer.h"

#include <iostream>

PluginTableWriter::PluginTableWriter(LlamaDB* db, const std::vector<PluginTableMeta>& tableMeta)
  : Conn(*db)
{
  for (const auto& tm : tableMeta) {
    duckdb_appender appender = nullptr;
    auto state = duckdb_appender_create(Conn.get(), nullptr, tm.tableName.c_str(), &appender);
    if (state == DuckDBError) {
      std::cerr << "Warning: failed to create appender for " << tm.tableName << "\n";
      continue;
    }
    Tables[tm.tableName] = {appender, tm.convertedSchema};
  }
}

PluginTableWriter::~PluginTableWriter() {
  for (auto& [name, ts] : Tables) {
    if (ts.appender) {
      duckdb_appender_destroy(&ts.appender);
    }
  }
}

int PluginTableWriter::write(const char* tableName, struct ArrowSchema* schema, struct ArrowArray* array) {
  (void)schema;  // schema is carried by the converted schema cached at init

  auto it = Tables.find(tableName);
  if (it == Tables.end()) {
    return -1;  // unknown table
  }

  duckdb_data_chunk chunk = nullptr;
  duckdb_error_data err = duckdb_data_chunk_from_arrow(
    Conn.get(), array, it->second.convertedSchema, &chunk
  );
  if (err) {
    std::cerr << "Warning: Arrow→DataChunk conversion failed for " << tableName
              << ": " << duckdb_error_data_message(err) << "\n";
    duckdb_destroy_error_data(&err);
    return -1;
  }

  auto state = duckdb_append_data_chunk(it->second.appender, chunk);
  duckdb_destroy_data_chunk(&chunk);

  if (state == DuckDBError) {
    std::cerr << "Warning: append failed for " << tableName << "\n";
    return -1;
  }

  return 0;
}

void PluginTableWriter::close() {
  for (auto& [name, ts] : Tables) {
    if (ts.appender) {
      duckdb_appender_close(ts.appender);
    }
  }
}

int PluginTableWriter::trampoline(void* opaque, const char* tableName,
                                  struct ArrowSchema* schema, struct ArrowArray* array) {
  return static_cast<PluginTableWriter*>(opaque)->write(tableName, schema, array);
}
```

- [ ] **Step 5: Add plugin_table_writer.cpp to src/meson.build**

In `src/meson.build`, add to the `llama_sources` list:

```
  'plugin_table_writer.cpp',
```

(Place it after `'pluginmanager.cpp'` for alphabetical order.)

- [ ] **Step 6: Add plugin_table_writer.cpp to test/meson.build common sources**

In `test/meson.build`, add to `llama_common_sources`:

```
  '../src/plugin_table_writer.cpp',
```

(Place it after `'../src/pluginmanager.cpp'`.)

- [ ] **Step 7: Build and run the test**

Run: `meson compile -C builddir && builddir/test/llama_test "testPluginTableWriterArrowBatch"`

Expected: PASS — the Arrow batch is converted, appended, and queryable.

- [ ] **Step 8: Write test for unknown table name**

Add to `test/test_pluginmanager.cpp`:

```cpp
TEST_CASE("testPluginTableWriterUnknownTable") {
  LlamaDB db;
  LlamaDBConnection conn(db);
  duckdb_query(conn.get(), "CREATE TABLE plugin_test_data (name VARCHAR, value UBIGINT);", nullptr);

  struct ArrowSchema name_schema{};
  name_schema.format = "u";
  name_schema.name = "name";
  name_schema.n_children = 0;
  name_schema.release = nullptr;

  struct ArrowSchema value_schema{};
  value_schema.format = "L";
  value_schema.name = "value";
  value_schema.n_children = 0;
  value_schema.release = nullptr;

  struct ArrowSchema* schema_children[] = {&name_schema, &value_schema};

  struct ArrowSchema batch_schema{};
  batch_schema.format = "+s";
  batch_schema.name = "";
  batch_schema.n_children = 2;
  batch_schema.children = schema_children;
  batch_schema.release = nullptr;

  duckdb_arrow_converted_schema converted = nullptr;
  duckdb_error_data err = duckdb_schema_from_arrow(conn.get(), &batch_schema, &converted);
  REQUIRE(!err);

  std::vector<PluginTableMeta> meta = {{"plugin_test_data", converted}};
  PluginTableWriter writer(&db, meta);

  // Try writing to a table that doesn't exist in the writer's map
  struct ArrowArray dummy{};
  int rc = writer.write("nonexistent_table", &batch_schema, &dummy);
  REQUIRE(rc < 0);

  writer.close();
  duckdb_destroy_arrow_converted_schema(&converted);
}
```

- [ ] **Step 9: Run both tests**

Run: `meson compile -C builddir && builddir/test/llama_test "testPluginTableWriter*"`

Expected: both PASS.

- [ ] **Step 10: Commit**

```bash
git add include/plugin_table_writer.h src/plugin_table_writer.cpp \
        test/test_pluginmanager.cpp src/meson.build test/meson.build
git commit -m "feat(plugin): add PluginTableWriter for Arrow→DuckDB ingestion

Per-Processor writer that receives Arrow record batches from plugins
via the write callback, converts them to DuckDB data chunks, and
appends them. Includes trampoline for the C FFI boundary."
```

---

### Task 7: Wire Up PluginTableWriter in Processor

Connect the PluginTableWriter to Processor's dispatch loop so plugins
can actually write Arrow data during processing.

**Files:**
- Modify: `include/processor.h`
- Modify: `src/processor.cpp`

- [ ] **Step 1: Add PluginTableWriter to Processor**

In `include/processor.h`, add include:

```cpp
#include "plugin_table_writer.h"
```

Add member to the `Processor` class (in the private section, after `SigAnalyzer`):

```cpp
  PluginTableWriter TableWriter;
```

- [ ] **Step 2: Initialize PluginTableWriter in Processor constructor**

In `src/processor.cpp`, update the Processor constructor initializer list.

Add after `SigAnalyzer(Context->SigProg, Context->SigMagics),`:

```cpp
  TableWriter(Context->Db, Context->Plugins.allTableMeta()),
```

- [ ] **Step 3: Update dispatch loop to use real write context**

In `src/processor.cpp`, replace the dummy write context:

Replace:
```cpp
    LlamaWriteContext writeCtx{};
    writeCtx.opaque = nullptr;
    writeCtx.write = nullptr;
```

With:
```cpp
    LlamaWriteContext writeCtx{};
    writeCtx.opaque = &TableWriter;
    writeCtx.write = PluginTableWriter::trampoline;
```

- [ ] **Step 4: Close PluginTableWriter in Processor::flush()**

In `src/processor.cpp`, at the end of `Processor::flush()`, add before the
closing brace:

```cpp
  TableWriter.close();
```

- [ ] **Step 5: Build and run all plugin tests**

Run: `meson compile -C builddir && builddir/test/llama_test "testPlugin*" "testProcessor*"`

Expected: all pass.

- [ ] **Step 6: Run the full test suite**

Run: `meson compile -C builddir && builddir/test/llama_test`

Expected: all tests pass.

- [ ] **Step 7: Commit**

```bash
git add include/processor.h src/processor.cpp
git commit -m "feat(plugin): wire PluginTableWriter into Processor dispatch

Processor constructs a PluginTableWriter from PluginManager's table
metadata, passes it as the write context to plugin process calls,
and closes it during flush."
```

---

### Task 8: Build System and Final Verification

Update Makefile.am for autotools, verify full build with both build
systems.

**Files:**
- Modify: `Makefile.am`

- [ ] **Step 1: Add new sources to Makefile.am**

In `Makefile.am`, add to `src_llama_common` (after `src/pluginmanager.cpp`
if present, or alphabetically):

```
	src/plugin_table_writer.cpp \
```

Note: `src/pluginmanager.cpp` may also be missing from `Makefile.am` (it's
in the meson build but not autotools). If so, add it too:

```
	src/pluginmanager.cpp \
	src/plugin_table_writer.cpp \
```

- [ ] **Step 2: Verify meson build is clean**

Run:
```bash
meson compile -C builddir 2>&1
```

Expected: clean build with no warnings related to the plugin changes.

- [ ] **Step 3: Run full test suite**

Run: `builddir/test/llama_test`

Expected: all tests pass.

- [ ] **Step 4: Commit**

```bash
git add Makefile.am
git commit -m "build: add plugin_table_writer to autotools build"
```
