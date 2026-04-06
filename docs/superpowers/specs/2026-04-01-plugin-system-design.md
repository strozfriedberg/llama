# Plugin System for Artifact Parsers

Runtime-loadable shared library plugins that provide file-format-specific forensic artifact parsing. Llama's core pipeline (hashing, grep, rule evaluation, file signatures) stays C++ and statically linked. Plugins add optional analysis capabilities — when present they receive files and write records to new database tables. When absent, llama operates as a fully static executable with no plugin overhead.

## Motivation

Forensic analysis requires file-format-specific artifact parsing (SQLite databases, Windows registry hives, plists, etc.). These parsers:

- Are numerous and growing — new formats are added regularly
- Benefit from Rust's memory safety, portability, and crate ecosystem
- Should not bloat or destabilize llama's core C++ binary
- Need to be optional — llama must work without them

## CLI

```
llama [OPTIONS] --plugin-dir <path> OUTPUT_DIRECTORY INPUT_FILE [INPUT_FILE ...]
```

- `--plugin-dir <path>` — path to a directory containing plugin shared libraries. Optional; if omitted, no plugins are loaded and no scanning occurs.
- No default search paths. No hardcoded paths. No environment variable fallback.

## C ABI Contract

All communication between llama (C++) and plugins crosses a C ABI boundary. The contract is defined in a single header, `include/plugin_api.h`, consumable by both C++ and Rust (via `bindgen` or hand-written FFI).

### Header

```c
#ifndef LLAMA_PLUGIN_API_H
#define LLAMA_PLUGIN_API_H

#include <stdint.h>
#include <stddef.h>

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
    int64_t   (*read)(void* opaque, uint8_t* buf, size_t len);
    int64_t   (*seek)(void* opaque, size_t pos);
    uint64_t  (*size)(void* opaque);
} LlamaReadSeek;

typedef struct {
    const char*   file_signature;   // NULL if no signature matched
    uint64_t      inode_addr;
    uint64_t      file_size;
    LlamaReadSeek readseek;
} LlamaFileContext;

typedef struct {
    const char* name;
    const char* version;
} LlamaPluginInfo;

// Plugin lifecycle
LLAMA_PLUGIN_EXPORT LlamaPluginInfo llama_plugin_init(void* duckdb_handle);
LLAMA_PLUGIN_EXPORT void            llama_plugin_shutdown(void);

// Per-file processing — returns 0 on success/skip, negative on error
LLAMA_PLUGIN_EXPORT int             llama_plugin_process(const LlamaFileContext* ctx);

// Error reporting — returns thread-local string describing last error
LLAMA_PLUGIN_EXPORT const char*     llama_plugin_last_error(void);

#ifdef __cplusplus
}
#endif

#endif // LLAMA_PLUGIN_API_H
```

### Function Semantics

**`llama_plugin_init(duckdb_handle)`** — Called once per plugin at startup. The `duckdb_handle` is a raw `duckdb_connection` pointer from DuckDB's C API. The plugin uses this connection to create its output tables (declaring schemas upfront so llama knows what tables exist). Returns plugin name and version. If init fails, the plugin should return a name of NULL and llama skips it.

**`llama_plugin_process(ctx)`** — Called for every file in the pipeline, from multiple threads concurrently. The plugin inspects `file_signature`, `inode_addr`, and `file_size` to decide whether to engage. If not interested, returns 0 immediately. If interested, reads file content via `readseek` and writes records to DuckDB. Returns 0 on success/skip, negative on error. Must be reentrant — no global mutable state.

**`llama_plugin_last_error()`** — Returns a pointer to a thread-local null-terminated error string. Valid until the next call to `process` on the same thread. Returns NULL if no error.

**`llama_plugin_shutdown()`** — Called once per plugin at teardown. Releases any resources allocated during init.

### File Context

The plugin receives:

| Field | Purpose |
|-------|---------|
| `file_signature` | Primary dispatch signal — e.g., "SQLite Database", "Windows Registry" |
| `inode_addr` | Key for joining back to llama's dirent/inode tables via DuckDB queries |
| `file_size` | Quick filtering without reading content |
| `readseek` | C function-pointer vtable for reading file content |

If a plugin needs additional metadata (timestamps, path, etc.), it queries DuckDB using `inode_addr` as the join key.

## ReadSeek Bridge

Llama's C++ `ReadSeek` interface is wrapped into the `LlamaReadSeek` C struct for crossing the ABI boundary.

### C++ Side (wrapping ReadSeek for plugins)

```cpp
LlamaReadSeek wrapReadSeek(ReadSeek* rs) {
    return LlamaReadSeek{
        .opaque = static_cast<void*>(rs),
        .read = [](void* o, uint8_t* buf, size_t len) -> int64_t {
            return static_cast<ReadSeek*>(o)->read(len, buf);
        },
        .seek = [](void* o, size_t pos) -> int64_t {
            static_cast<ReadSeek*>(o)->seek(pos);
            return 0;
        },
        .size = [](void* o) -> uint64_t {
            return static_cast<ReadSeek*>(o)->size();
        }
    };
}
```

### Rust Side (consuming LlamaReadSeek as Read + Seek)

Plugin authors wrap `LlamaReadSeek` into a struct implementing `std::io::Read + std::io::Seek`, giving them an idiomatic Rust I/O handle usable with any crate expecting standard traits.

## PluginManager (C++ Side)

A `PluginManager` class owns plugin loading, lifecycle, and dispatch.

### Responsibilities

- **`loadPlugins(pluginDir, duckdb_handle)`** — Scans directory for shared libraries (`.so` on Linux, `.dylib` on macOS, `.dll` on Windows) using `std::filesystem::directory_iterator`. For each library: loads via `boost::dll::shared_library`, resolves the 4 exported symbols, calls `llama_plugin_init(duckdb_handle)`, stores the loaded plugin. If a library fails to load or is missing symbols, logs a warning and skips it.

- **`processFile(ctx)`** — Iterates all loaded plugins, calls `llama_plugin_process(&ctx)` on each. On negative return, retrieves `llama_plugin_last_error()` and logs to the `exception_log` table with plugin name.

- **`shutdown()`** — Calls `llama_plugin_shutdown()` on each plugin in reverse load order, then unloads shared libraries.

### Loaded Plugin State

Each loaded plugin is represented as:

```cpp
struct LoadedPlugin {
    std::string name;
    std::string version;
    boost::dll::shared_library library;
    std::function<int(const LlamaFileContext*)>  process;
    std::function<const char*()>                 lastError;
    std::function<void()>                        shutdown;
};
```

## Pipeline Integration

### Processor Changes

`Processor` gains a `PluginManager*` member (nullable). After the existing analysis stages complete (hashing, grep, rules, file sigs), if `pluginManager` is non-null:

1. Seek `ReadSeek` back to position 0.
2. Build `LlamaFileContext` from existing results (signature from `FileSigAnalyzer`, inode addr from current entry, file size, wrapped `ReadSeek`).
3. Call `pluginManager->processFile(ctx)`.
4. On error, log to `exception_log` with plugin name and error message.

The `ReadSeek` handle is seeked back to 0 before each plugin's process call as well, so plugins don't need to worry about position left by a previous plugin.

### Initialization Order

1. Parse CLI args (including `--plugin-dir`).
2. Open DuckDB database.
3. If `--plugin-dir` provided, construct `PluginManager`, call `loadPlugins()`. Plugins create their tables during init.
4. Build processor pool. Each `Processor` clone receives a pointer to the shared `PluginManager`.
5. Process files normally.
6. After all processing completes, call `PluginManager::shutdown()`.
7. Close DuckDB.

### No-Plugin Path

If `--plugin-dir` is not specified, `PluginManager` is never constructed. `Processor` sees a null pointer and skips the plugin step. Zero overhead beyond a null check.

## Threading Model

- Plugins must be reentrant. Llama calls `llama_plugin_process` from multiple threads concurrently.
- Plugins must not maintain mutable global state. If they do, thread safety is their responsibility.
- DuckDB connections are thread-safe. Plugins use DuckDB appenders or prepared statements for writes.
- `llama_plugin_last_error` returns a thread-local string.

## Build & Platform

### Boost.DLL

Add Boost.DLL to the build. It is header-only in modern Boost, so this is an include path addition — no new linked library. Llama already wraps Boost headers in shim headers for warning suppression; `include/boost_dll.h` follows this pattern (already created).

### Plugin API Header

`include/plugin_api.h` is a pure C header guarded with `extern "C"`. The `LLAMA_PLUGIN_EXPORT` macro handles symbol visibility portably:
- Linux/macOS: `__attribute__((visibility("default")))`
- Windows: `__declspec(dllexport)`

Note: `LLAMA_PLUGIN_EXPORT` is used by plugin implementations to mark their exported symbols. Llama itself does not call these functions through the header declarations — it resolves them at runtime via Boost.DLL. The header serves as documentation of the ABI contract and as an include for plugin authors.

### Shared Library Extensions

`PluginManager` filters directory entries by platform extension, determined at compile time:

| Platform | Extension |
|----------|-----------|
| Linux    | `.so`     |
| macOS    | `.dylib`  |
| Windows  | `.dll`    |

### Test Stub Plugin

A minimal C plugin built as a shared library for integration testing:

```c
#include "plugin_api.h"
#include <string.h>

static _Thread_local const char* last_err = NULL;

LLAMA_PLUGIN_EXPORT LlamaPluginInfo llama_plugin_init(void* duckdb_handle) {
    return (LlamaPluginInfo){ .name = "test-plugin", .version = "0.1.0" };
}

LLAMA_PLUGIN_EXPORT int llama_plugin_process(const LlamaFileContext* ctx) {
    if (ctx->file_signature && strcmp(ctx->file_signature, "SQLite") == 0) {
        return 0;
    }
    return 0;
}

LLAMA_PLUGIN_EXPORT const char* llama_plugin_last_error(void) { return last_err; }

LLAMA_PLUGIN_EXPORT void llama_plugin_shutdown(void) {}
```

Built as a `shared_library()` target in Meson, only when tests are enabled. Integration tests point `--plugin-dir` at the build output directory containing this stub.

### Meson Changes

- Add `boost_dll` dependency (or just include path if header-only).
- Add `plugin_api.h` to installed headers.
- Add `shared_library('test_plugin', ...)` under test configuration.
