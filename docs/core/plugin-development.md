# Plugin Development Guide

This guide covers everything you need to write, build, and deploy a llama plugin.
Plugins are shared libraries (`.so`, `.dylib`, or `.dll`) that llama loads at
runtime to parse forensic artifacts -- SQLite databases, browser history,
email archives, or any file format your investigation requires.

## Table of Contents

- [Architecture Overview](#architecture-overview)
- [The C ABI Contract](#the-c-abi-contract)
- [Plugin Lifecycle](#plugin-lifecycle)
- [ReadSeek I/O Interface](#readseek-io-interface)
- [Writing a Plugin](#writing-a-plugin)
- [Building a Plugin](#building-a-plugin)
- [Using Plugins](#using-plugins)
- [Current Limitations](#current-limitations)

---

## Architecture Overview

Llama's plugin system is a runtime-loadable shared library architecture.
At startup, if a plugin directory is specified, llama scans it for shared
libraries matching the platform extension (`.so` on Linux, `.dylib` on macOS,
`.dll` on Windows). For each library found, llama:

1. Opens the shared library via dynamic loading.
2. Resolves four required exported symbols (`llama_plugin_init`,
   `llama_plugin_process`, `llama_plugin_last_error`, `llama_plugin_shutdown`).
3. Calls `llama_plugin_init()`, passing a DuckDB connection handle so the plugin
   can create tables or perform other database setup.
4. Stores the plugin for dispatch during file processing.

Libraries that are missing any of the four required symbols are skipped with a
warning.

During forensic evidence processing, llama's pipeline processes each file
through several stages: hashing, file signature detection, **plugin dispatch**,
and keyword search -- in that order. After signature detection, llama constructs
a `LlamaFileContext` containing the first matched signature, the file's inode
address, its size, and a `LlamaReadSeek` I/O vtable, then calls every loaded
plugin's `process()` function with that context.

When llama shuts down, it calls each plugin's `shutdown()` function in reverse
load order.

If no `--plugin-dir` is specified, the plugin subsystem is never constructed and
adds zero overhead beyond a null pointer check in the processing loop.

---

## The C ABI Contract

The plugin interface is defined in
[`include/plugin_api.h`](../../include/plugin_api.h). It uses a pure C ABI for
two reasons:

- **Language interoperability.** A C ABI can be implemented from C, C++, Rust,
  Zig, or any language with C FFI support.
- **ABI stability.** C structs and function signatures have a stable ABI across
  compiler versions and vendors. There is no name mangling, no vtable layout
  variation, and no STL versioning to worry about.

### Export Macro

```c
#ifdef _WIN32
  #define LLAMA_PLUGIN_EXPORT __declspec(dllexport)
#else
  #define LLAMA_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif
```

Every exported function must be prefixed with `LLAMA_PLUGIN_EXPORT` to ensure
the symbol is visible in the shared library. On non-Windows platforms this
requires either `-fvisibility=default` at compile time or explicit use of this
macro (which overrides per-symbol visibility regardless of the default).

### Structs

#### `LlamaReadSeek`

```c
typedef struct {
    void*     opaque;
    int64_t   (*read)(void* opaque, uint8_t* buf, size_t len);
    int64_t   (*seek)(void* opaque, size_t pos);
    uint64_t  (*size)(void* opaque);
} LlamaReadSeek;
```

A vtable for reading file content. The `opaque` pointer is managed by llama
and must be passed through to each function pointer. See
[ReadSeek I/O Interface](#readseek-io-interface) for usage details.

#### `LlamaFileContext`

```c
typedef struct {
    const char*   file_signature;   /* NULL if no signature matched */
    uint64_t      inode_addr;
    uint64_t      file_size;
    LlamaReadSeek readseek;
} LlamaFileContext;
```

Per-file context passed to `llama_plugin_process()`. Fields:

| Field            | Description                                                    |
|------------------|----------------------------------------------------------------|
| `file_signature` | Name of the first matched file signature (e.g., `"SQLite Database"`), or `NULL` if no signature matched. |
| `inode_addr`     | The filesystem inode address (metadata address) of the file.   |
| `file_size`      | Total size of the file in bytes.                               |
| `readseek`       | I/O vtable for reading the file's content.                     |

#### `LlamaPluginInfo`

```c
typedef struct {
    const char* name;
    const char* version;
} LlamaPluginInfo;
```

Returned by `llama_plugin_init()`. The `name` field must not be `NULL` -- if it
is, the plugin is skipped. The `version` field may be `NULL` (defaults to
`"unknown"` in log output).

### Required Exports

Every plugin must export exactly four functions:

#### `llama_plugin_init`

```c
LLAMA_PLUGIN_EXPORT LlamaPluginInfo llama_plugin_init(void* duckdb_handle);
```

Called once at load time. `duckdb_handle` is a pointer to a `duckdb_connection`
-- cast it to `duckdb_connection*` to use the DuckDB C API for creating tables,
preparing statements, or other setup work.

Return a `LlamaPluginInfo` with at minimum a non-NULL `name`.

**Important:** This connection is shared and should only be used during init.
If your plugin needs database access during `process()`, create your own
connections (see [Threading Model](#threading-model)).

#### `llama_plugin_process`

```c
LLAMA_PLUGIN_EXPORT int llama_plugin_process(const LlamaFileContext* ctx);
```

Called once per file during evidence processing. Return `0` on success or to
skip the file (e.g., unrecognized signature). Return a negative value on error.

On error, llama calls `llama_plugin_last_error()` to retrieve a description and
logs it to the `exception_log` table.

**This function is called from multiple threads concurrently.** See
[Threading Model](#threading-model).

#### `llama_plugin_last_error`

```c
LLAMA_PLUGIN_EXPORT const char* llama_plugin_last_error(void);
```

Return a string describing the last error on the calling thread. This should
be thread-local storage. Return `NULL` if no error has occurred.

#### `llama_plugin_shutdown`

```c
LLAMA_PLUGIN_EXPORT void llama_plugin_shutdown(void);
```

Called once when llama exits. Release resources, close connections, and clean up
global state. Plugins are shut down in reverse load order. Exceptions thrown
during shutdown are caught and silently ignored by llama.

---

## Plugin Lifecycle

```
                    +------------------------------------------+
                    |              llama startup                |
                    +------------------------------------------+
                                      |
                                      v
                    +------------------------------------------+
                    | llama_plugin_init(duckdb_handle)          |
                    |   - Create tables                        |
                    |   - Store config                         |
                    |   - Return name + version                |
                    +------------------------------------------+
                                      |
                                      v
                    +------------------------------------------+
                    | llama_plugin_process(ctx)                 |
                    |   - Called per file, from worker threads  |
                    |   - Check ctx->file_signature             |
                    |   - Read file via ctx->readseek           |
                    |   - Write results to DuckDB               |
                    +------------------------------------------+
                           |         |          |
                        thread 1  thread 2  thread N  ...
                                      |
                                      v
                    +------------------------------------------+
                    | llama_plugin_shutdown()                   |
                    |   - Release resources                    |
                    |   - Close connections                    |
                    +------------------------------------------+
```

### Initialization

`llama_plugin_init()` is called once, on the main thread, immediately after
the shared library is loaded. The `duckdb_handle` argument is a
`duckdb_connection*`. Use it to create tables for your plugin's output:

```c
#include <duckdb.h>

LLAMA_PLUGIN_EXPORT LlamaPluginInfo llama_plugin_init(void* duckdb_handle) {
    duckdb_connection* conn = (duckdb_connection*)duckdb_handle;
    duckdb_query(*conn,
        "CREATE TABLE IF NOT EXISTS my_artifacts ("
        "  inode_addr UBIGINT, key TEXT, value TEXT"
        ")", NULL);

    LlamaPluginInfo info;
    info.name = "my-artifact-parser";
    info.version = "1.0.0";
    return info;
}
```

### Processing

`llama_plugin_process()` is called once per file after file signature detection.
The file's read cursor is positioned at offset 0 when your function is called.

A typical implementation checks the signature, reads the file content, parses
it, and writes results to DuckDB:

```c
LLAMA_PLUGIN_EXPORT int llama_plugin_process(const LlamaFileContext* ctx) {
    if (!ctx->file_signature) {
        return 0;  /* no signature, skip */
    }
    if (strcmp(ctx->file_signature, "SQLite Database") != 0) {
        return 0;  /* not our type, skip */
    }

    /* Read and process the file... */
    return 0;
}
```

### Shutdown

`llama_plugin_shutdown()` is called once on the main thread after all processing
is complete. Clean up any global or thread-local resources.

### Threading Model

Llama processes files using a pool of worker threads. Each worker thread calls
`llama_plugin_process()` independently, so **multiple concurrent calls to
`process()` will happen simultaneously**.

Your plugin must be thread-safe. Practical guidelines:

- **No shared mutable state.** If you need per-call scratch space, use local
  variables or thread-local storage.
- **Thread-local error strings.** Use `_Thread_local` (C11) or
  `thread_local` (C++11) for the last-error string:

  ```c
  static _Thread_local const char* last_err = NULL;
  ```

- **Database connections.** The `duckdb_handle` passed to `init()` is a single
  connection. Do not use it from `process()` -- DuckDB connections are not
  thread-safe. Instead, open the DuckDB database during init and create
  per-thread connections using `_Thread_local`:

  ```c
  static duckdb_database db_handle;

  static _Thread_local duckdb_connection thread_conn;
  static _Thread_local int conn_initialized = 0;

  static duckdb_connection get_thread_conn(void) {
      if (!conn_initialized) {
          duckdb_connect(db_handle, &thread_conn);
          conn_initialized = 1;
      }
      return thread_conn;
  }
  ```

---

## ReadSeek I/O Interface

The `LlamaReadSeek` vtable in `LlamaFileContext` provides random-access
read-only I/O into the file being processed. The file may live inside a
forensic disk image (E01, raw, etc.) -- the vtable abstracts this away.

### Functions

All three functions take the `opaque` pointer from the `LlamaReadSeek` struct
as their first argument. Always pass `ctx->readseek.opaque`.

#### `read(opaque, buf, len)`

```c
int64_t (*read)(void* opaque, uint8_t* buf, size_t len);
```

Read up to `len` bytes into `buf` starting at the current position.

- Returns the number of bytes actually read (>= 0) on success.
- Returns `-1` on error.
- A return of `0` indicates end-of-file.
- Advances the current position by the number of bytes read.

#### `seek(opaque, pos)`

```c
int64_t (*seek)(void* opaque, size_t pos);
```

Set the current read position to absolute byte offset `pos`.

- Returns `0` on success.
- Returns `-1` on error (e.g., seeking past end of file).

#### `size(opaque)`

```c
uint64_t (*size)(void* opaque);
```

Returns the total size of the file in bytes. This matches `ctx->file_size`.

### Usage Example

```c
/* Read the entire file into a malloc'd buffer */
uint64_t total = ctx->readseek.size(ctx->readseek.opaque);
uint8_t* data = malloc(total);
if (!data) {
    last_err = "allocation failed";
    return -1;
}

ctx->readseek.seek(ctx->readseek.opaque, 0);

size_t offset = 0;
while (offset < total) {
    int64_t n = ctx->readseek.read(ctx->readseek.opaque,
                                    data + offset,
                                    total - offset);
    if (n < 0) {
        free(data);
        last_err = "read error";
        return -1;
    }
    if (n == 0) break;  /* EOF */
    offset += (size_t)n;
}

/* ... parse data ... */

free(data);
```

### Error Handling

The ReadSeek implementation wraps llama's internal I/O layer. If the underlying
forensic image read throws an exception, the wrapper catches it and returns
`-1`. Always check return values.

---

## Writing a Plugin

Here is a complete, minimal plugin that detects SQLite databases and reads their
first 16 bytes. This mirrors the test stub at
[`test/test_plugin_stub.c`](../../test/test_plugin_stub.c).

### Complete Example: `sqlite_check.c`

```c
#include "plugin_api.h"
#include <string.h>

/* Thread-local error string */
static _Thread_local const char* last_err = NULL;

/*
 * Init: called once at startup.
 * duckdb_handle is a duckdb_connection* -- use it to create tables.
 * We don't need DB access in this minimal example, so we ignore it.
 */
LLAMA_PLUGIN_EXPORT LlamaPluginInfo llama_plugin_init(void* duckdb_handle) {
    (void)duckdb_handle;
    LlamaPluginInfo info;
    info.name = "sqlite-check";
    info.version = "0.1.0";
    return info;
}

/*
 * Process: called per file, from multiple threads concurrently.
 * Return 0 to indicate success/skip, negative on error.
 */
LLAMA_PLUGIN_EXPORT int llama_plugin_process(const LlamaFileContext* ctx) {
    /* Only handle files identified as SQLite databases */
    if (!ctx->file_signature || strcmp(ctx->file_signature, "SQLite Database") != 0) {
        return 0;  /* not our type, skip */
    }

    /* Read the SQLite header (first 16 bytes) */
    uint8_t buf[16];
    int64_t n = ctx->readseek.read(ctx->readseek.opaque, buf, sizeof(buf));
    if (n < 0) {
        last_err = "read failed";
        return -1;
    }
    if (n < 16) {
        last_err = "short read on SQLite header";
        return -1;
    }

    /* Verify the SQLite magic string: "SQLite format 3\000" */
    if (memcmp(buf, "SQLite format 3\000", 16) != 0) {
        last_err = "not a valid SQLite file";
        return -1;
    }

    /* Success -- in a real plugin you would parse the DB and write results */
    return 0;
}

/*
 * Last error: return thread-local error string, or NULL if none.
 */
LLAMA_PLUGIN_EXPORT const char* llama_plugin_last_error(void) {
    return last_err;
}

/*
 * Shutdown: called once at exit. Clean up global state.
 */
LLAMA_PLUGIN_EXPORT void llama_plugin_shutdown(void) {
    last_err = NULL;
}
```

### Checklist

Before considering your plugin complete, verify:

- [ ] All four functions are exported with `LLAMA_PLUGIN_EXPORT`.
- [ ] `llama_plugin_init()` returns a `LlamaPluginInfo` with a non-NULL `name`.
- [ ] `llama_plugin_process()` is thread-safe (no shared mutable state).
- [ ] Error strings are stored in `_Thread_local` storage.
- [ ] `llama_plugin_process()` returns `0` for files it does not handle.
- [ ] `llama_plugin_process()` returns a negative value on error.
- [ ] ReadSeek return values are checked.
- [ ] `llama_plugin_shutdown()` cleans up all resources.

---

## Building a Plugin

Plugins are compiled as shared libraries and must export the four required
symbols with C linkage.

### Linux (GCC)

```bash
gcc -shared -fPIC -fvisibility=default \
    -o sqlite_check.so \
    sqlite_check.c \
    -I /path/to/llama/include
```

### macOS (Clang)

```bash
clang -shared -fPIC -fvisibility=default \
    -o sqlite_check.dylib \
    sqlite_check.c \
    -I /path/to/llama/include
```

### Key Compiler Flags

| Flag                      | Purpose                                                      |
|---------------------------|--------------------------------------------------------------|
| `-shared`                 | Produce a shared library instead of an executable.           |
| `-fPIC`                   | Generate position-independent code (required for shared libs). |
| `-fvisibility=default`    | Make all symbols visible by default. Alternatively, rely on the `LLAMA_PLUGIN_EXPORT` macro for per-symbol visibility and omit this flag. |
| `-I /path/to/llama/include` | Ensure the compiler can find `plugin_api.h`.               |

### C++ Plugins

If you write your plugin in C++, wrap the exports in `extern "C"` to prevent
name mangling. The `plugin_api.h` header already includes `extern "C"` guards,
so simply including it and using `LLAMA_PLUGIN_EXPORT` is sufficient:

```cpp
#include "plugin_api.h"

// C++ code here -- the extern "C" block in plugin_api.h
// ensures the four required functions have C linkage.
```

Compile with a C++ compiler:

```bash
g++ -shared -fPIC -fvisibility=default \
    -o my_plugin.so \
    my_plugin.cpp \
    -I /path/to/llama/include
```

### Linking Against DuckDB

If your plugin uses the DuckDB C API (e.g., to insert rows during `process()`),
link against the DuckDB library:

```bash
gcc -shared -fPIC -fvisibility=default \
    -o my_plugin.so \
    my_plugin.c \
    -I /path/to/llama/include \
    -I /path/to/duckdb/include \
    -L /path/to/duckdb/lib -lduckdb
```

### Verifying Exports

After building, verify your plugin exports the required symbols:

```bash
# Linux
nm -D sqlite_check.so | grep llama_plugin

# macOS
nm sqlite_check.dylib | grep llama_plugin
```

You should see four symbols:

```
T llama_plugin_init
T llama_plugin_last_error
T llama_plugin_process
T llama_plugin_shutdown
```

If any are missing, llama will log a warning and skip the library.

---

## Using Plugins

Pass the `--plugin-dir` flag to llama with the path to a directory containing
your plugin shared libraries:

```bash
llama --plugin-dir /path/to/plugins output_dir evidence.E01
```

Llama scans the directory for files matching the platform's shared library
extension (`.so`, `.dylib`, or `.dll`). Non-matching files are ignored.
Subdirectories are not searched.

On successful load, llama prints each plugin's name and version to stderr:

```
Loaded plugin: sqlite-check v0.1.0
Loaded plugin: browser-history v2.3.1
```

If `--plugin-dir` is omitted, no plugins are loaded and the plugin dispatch
step is skipped entirely.

---

## Current Limitations

- **Only the first file signature is passed.** If a file matches multiple
  signatures, only the first is provided in `ctx->file_signature`. Plugins
  cannot see the full list of matched signatures.

- **No plugin-to-plugin communication.** Plugins cannot discover or call other
  plugins. Each plugin operates independently on the file context it receives.

- **Single init connection.** The `duckdb_handle` passed to `llama_plugin_init()`
  is a single shared connection suitable only for setup work (creating tables,
  etc.). Plugins that need database access during `process()` must obtain their
  own per-thread connections from the DuckDB database.

- **No plugin ordering guarantees.** Plugins are loaded in filesystem iteration
  order and called in load order. There is no mechanism to specify dependencies
  or execution order between plugins.

- **No hot reloading.** Plugins are loaded once at startup and unloaded at
  shutdown. Changing a plugin requires restarting llama.

- **Errors halt the plugin chain.** If a plugin's `process()` returns a negative
  value for a given file, no further plugins are called for that file. The error
  is logged and processing continues with the next file.
