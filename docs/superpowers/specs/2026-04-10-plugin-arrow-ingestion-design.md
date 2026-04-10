# Plugin Arrow Ingestion: Llama-Side Design

## Problem

The current plugin API passes a `duckdb_connection` to the plugin during init.
The plugin links its own DuckDB (via `libduckdb-sys`) and uses it to create
tables and insert rows. This causes a fatal ABI conflict: the plugin's bundled
DuckDB has different internals than llama's system DuckDB, so using llama's
connection handle with the plugin's DuckDB functions segfaults.

The solution (see `findallevidence/docs/superpowers/specs/2026-04-10-plugin-api-arrow-redesign.md`
for the full cross-project spec) replaces DuckDB interaction in the plugin with
the Apache Arrow C Data Interface. The plugin produces Arrow record batches and
flushes them to llama via a callback. Llama handles all DuckDB operations.

This spec covers the llama-side changes only. The findallevidence plugin-side
changes are in-flight separately.

## Design

### C ABI Changes (`plugin_api.h`)

Three new types and two updated function signatures.

New types:

```c
#include "arrow/c/abi.h"  /* vendored, see note below */

/* A (table_name, schema) pair declared by the plugin during init. */
typedef struct {
    const char*          table_name;
    struct ArrowSchema   schema;   /* release = NULL → static lifetime */
} LlamaTableDef;

/* Callback for the plugin to flush Arrow record batches to llama. */
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
```

Updated signatures:

```c
/* Init: no longer receives a DuckDB handle. Returns table declarations
   via out-params. The tables array has static lifetime (plugin owns it). */
int llama_plugin_init(
    LlamaPluginInfo*      info,
    const LlamaTableDef** tables,
    size_t*               num_tables
);

/* Process: now receives a write context for flushing Arrow batches. */
int llama_plugin_process(
    const LlamaFileContext*  ctx,
    const LlamaWriteContext* write_ctx,
    const char**             errmsg
);
```

Unchanged: `LlamaReadSeek`, `LlamaFileContext`, `LlamaPluginInfo`,
`llama_plugin_free_error`, `llama_plugin_shutdown`.

### Vendored Arrow C Data Interface Header

The Arrow C Data Interface defines two C structs (`ArrowSchema` and
`ArrowArray`) whose layout is frozen and will never change. The header is
~50 lines, Apache-licensed, and has no dependencies.

We vendor it as `include/arrow/c/abi.h` rather than requiring a system
Arrow installation.

**Upstream source:** https://arrow.apache.org/docs/format/CDataInterface.html
— the canonical struct definitions are in the specification itself.

If the upstream structs are ever revised (unlikely given the frozen ABI
guarantee), update the vendored copy from the specification.

### PluginManager

`loadPlugins(pluginDir, dbConn)` keeps its public signature. Internal
changes:

1. Loads the library and resolves symbols (same as today, but with new
   function signatures).
2. Calls `llama_plugin_init(&info, &tables, &num_tables)` — no DuckDB
   handle passed to the plugin.
3. For each `LlamaTableDef` returned by the plugin:
   - Calls `duckdb_schema_from_arrow(dbConn, &schema, &converted)` to
     convert the Arrow schema to DuckDB types.
   - Builds and executes a `CREATE TABLE` statement from the converted
     types and table name.
   - Caches the `duckdb_arrow_converted_schema` alongside the table name
     in the `LoadedPlugin`.
4. Stores the plugin with its table metadata.

`LoadedPlugin` gains:
- Updated `process` function pointer signature (includes `LlamaWriteContext*`).
- A list of table metadata entries: table name + cached
  `duckdb_arrow_converted_schema`.

`shutdown()` adds cleanup of cached `duckdb_arrow_converted_schema` handles
via `duckdb_destroy_arrow_converted_schema()`.

PluginManager exposes the per-plugin table metadata so that
PluginTableWriter can be constructed from it.

### PluginTableWriter

New type in `include/plugin_table_writer.h` / `src/plugin_table_writer.cpp`.
Created per-Processor, one instance covering all plugins that declared
tables.

**What it holds:**
- Its own `LlamaDBConnection` (from the shared `LlamaDB*`) — needed for
  `duckdb_data_chunk_from_arrow()`.
- A map of table name to appender and borrowed converted schema pointer.
  Appenders are owned; converted schemas are borrowed from PluginManager
  (which outlives all Processors).

**What it does:**
- `write(table_name, schema, array)` — the core method:
  1. Looks up the table by name; returns error if unknown.
  2. Calls `duckdb_data_chunk_from_arrow(conn, array, converted_schema, &chunk)`.
  3. Calls `duckdb_append_data_chunk(appender, chunk)`.
  4. Calls `duckdb_destroy_data_chunk(&chunk)`.
  5. Returns 0 on success, negative on error.
- A static trampoline function that casts the `void* opaque` to
  `PluginTableWriter*` and calls `write()`. Required for the C FFI
  boundary.
- `close()` — calls `duckdb_appender_close()` on each appender. Called
  from `Processor::flush()`.
- Destructor calls `duckdb_appender_destroy()` to free appender memory.

**Threading:** Each Processor (and therefore each PluginTableWriter) is
per-thread. No locking needed. DuckDB may do its own internal
synchronization but that is not our concern.

### Processor Changes

Minimal changes to Processor:

- Gains a `PluginTableWriter` member. Constructed from PluginManager's
  table metadata and Processor's `LlamaDB*`.
- In the dispatch loop (currently `processor.cpp:168-199`):
  1. Constructs a `LlamaWriteContext` on the stack with
     `opaque = &tableWriter` and
     `write = PluginTableWriter::trampoline`.
  2. Calls `plugin.process(&pluginCtx, &writeCtx, &errmsg)`.
- `Processor::flush()` calls `tableWriter.close()`.
- `Processor::clone()` creates a new PluginTableWriter for the cloned
  Processor (each clone gets its own connection and appenders).

Everything else in Processor is unchanged: hash dispatch, signature
analysis, search, exception logging.

### Test Changes

- **Stub plugins** (`test_plugin_stub.c`, `test_plugin_error_stub.c`) —
  updated for new signatures. Return 0 tables and ignore the write
  context (they test lifecycle and error handling, not Arrow ingestion).
- **`test_pluginmanager.cpp`** — `loadPlugins()` still passes a
  `duckdb_connection`. Add verification that tables declared by plugins
  get created in DuckDB.
- **New test** — end-to-end write path: construct a PluginTableWriter,
  call the write callback with a hand-built Arrow batch, verify the data
  lands in DuckDB.

The vendored `arrow/c/abi.h` is just two struct definitions, so test
stubs can include it without pulling in any library.

### Build System

**Autotools (`configure.ac`, `Makefile.am`):**
- Add `plugin_table_writer.h`/`.cpp` to the build.
- Add `include/arrow/c/abi.h` to distributed headers.
- No new library linkage — DuckDB already has the Arrow ingestion
  functions, and the Arrow C Data Interface is header-only.

**Meson (`meson.build`, `src/meson.build`, `test/meson.build`):**
- Add `plugin_table_writer.cpp` to source lists.
- Add `include/arrow/c/` to include directories if needed.
- Same: no new library dependencies.

## CRC Cards

### PluginManager

**Responsibilities:**
- Loads plugin libraries from a directory
- Validates that plugins export required symbols
- Initializes plugins and collects table declarations
- Creates DuckDB tables from declared schemas
- Caches converted schemas for later use
- Shuts down plugins in reverse order
- Cleans up cached schema handles

**Collaborators:**
- LoadedPlugin — stores per-plugin metadata and table info
- PluginTableWriter — borrows cached schemas to create writers

### LoadedPlugin

**Responsibilities:**
- Holds plugin identity (name, version)
- Holds plugin function pointers (process, free error, shutdown)
- Carries table metadata (names and converted schemas)

**Collaborators:**
- PluginManager — created and owned by manager

### Processor

**Responsibilities:**
- Builds per-file context for plugin dispatch
- Constructs write context pointing to table writer
- Dispatches files to each plugin's process function
- Handles plugin errors and exceptions
- Closes table writer during flush

**Collaborators:**
- PluginManager — reads plugin list and table metadata
- PluginTableWriter — provides write callback for plugin dispatch

### PluginTableWriter

**Responsibilities:**
- Receives Arrow batches via write callback
- Validates table name against declared tables
- Converts Arrow batches to DuckDB data chunks
- Appends data chunks to table appenders
- Closes all appenders when processing completes

**Collaborators:**
- Processor — created per-processor, called during dispatch and flush
- PluginManager — borrows converted schemas from cached table metadata
