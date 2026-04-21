# Plugin System Fixes

Post-review fixes to the plugin system implemented in the `readseek-contract-hardening` branch. These changes address correctness, API clarity, and agent-legibility issues identified in `docs/notes/plugin-system-code-review.md`. The original design is in `docs/superpowers/specs/2026-04-01-plugin-system-design.md`.

## 1. PluginManager Threading Model

**Problem:** The thread-safety of `PluginManager::processFile()` is correct but not self-evident. Agents analyzing the code repeatedly get confused by the implicit lifecycle ordering before concluding the code is safe. The `processFile()` method also conflates iteration logic with plugin lifecycle management.

**Changes:**

Remove `processFile()` from `PluginManager`. Plugin iteration moves into `Processor::process()`, where error handling via `logException()` is natural.

`PluginManager` becomes a pure lifecycle manager:

```cpp
class PluginManager {
public:
  // Thread-safety contract:
  //   - loadPlugins() and shutdown() are single-threaded (main thread).
  //   - plugins() is const and safe to call concurrently from processing threads.
  //   - The Plugins vector is never mutated while processing threads are active.
  //   Lifecycle: loadPlugins() -> [processing...] -> shutdown()

  void loadPlugins(const std::filesystem::path& pluginDir, duckdb_connection dbConn);
  void shutdown();

  const std::vector<LoadedPlugin>& plugins() const { return Plugins; }
  bool empty() const { return Plugins.empty(); }
  size_t pluginCount() const { return Plugins.size(); }

  // ...
};
```

`Llama` always constructs a `PluginManager` (not a conditional `unique_ptr`). When `--plugin-dir` is not provided, the manager has zero plugins and `empty()` returns true.

`ProcessorContext` holds `const PluginManager&` instead of `PluginManager*`. All `Processor` clones see the plugin manager as const through the shared `ProcessorContext`.

Plugin dispatch in `Processor::process()`:

```cpp
if (!Context->Plugins.empty()) {
    const char* sigName = sigResults.empty() ? nullptr : sigResults[0]->Name.c_str();
    LlamaFileContext pluginCtx{};
    pluginCtx.struct_size = sizeof(LlamaFileContext);
    pluginCtx.file_signature = sigName;
    pluginCtx.inode_addr = entry.Addr;
    pluginCtx.file_size = entry.FileSize;
    pluginCtx.readseek = wrapReadSeek(&entry.getStream());

    for (const auto& plugin : Context->Plugins.plugins()) {
        pluginCtx.readseek.seek(pluginCtx.readseek.opaque, 0);
        const char* errmsg = nullptr;
        int rc;
        try {
            rc = plugin.process(&pluginCtx, &errmsg);
        } catch (const std::exception& e) {
            logException(entry, ("plugin:" + plugin.name).c_str(), e.what());
            continue;
        } catch (...) {
            logException(entry, ("plugin:" + plugin.name).c_str(), "plugin threw unknown exception");
            continue;
        }
        if (rc < 0) {
            logException(entry, ("plugin:" + plugin.name).c_str(),
                         errmsg ? errmsg : "unknown error");
            if (errmsg) plugin.freeError(errmsg);
        }
    }
}
```

This addresses:
- **C-1:** `const PluginManager&` in `ProcessorContext` makes thread-safety self-evident at the type level.
- **I-2:** Plugin calls are wrapped in try/catch.
- **I-3:** All plugins run; errors are logged independently via `logException()`.

## 2. Error Reporting Redesign

**Problem:** `llama_plugin_last_error()` requires plugins to use thread-local storage. The documentation is not strong enough about this requirement, and the pattern is a TOCTOU footgun for plugin authors.

**Changes:**

Remove `llama_plugin_last_error()`. Error reporting moves inline into `llama_plugin_process()` via an out-parameter, with a separate free function for cross-heap safety (Rust plugins use a different allocator).

```c
// Per-file processing -- returns 0 on success/skip, negative on error.
// On error, *errmsg is set to a plugin-allocated string describing the error.
// Caller must pass *errmsg to llama_plugin_free_error() when done.
// Called from multiple threads concurrently -- plugins must be thread-safe.
LLAMA_PLUGIN_EXPORT int  llama_plugin_process(const LlamaFileContext* ctx, const char** errmsg);

// Free an error string returned via llama_plugin_process.
LLAMA_PLUGIN_EXPORT void llama_plugin_free_error(const char* errmsg);
```

`LoadedPlugin` replaces the `lastError` member with `freeError`:

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

Symbol resolution in `loadPlugins()` checks for `llama_plugin_free_error` instead of `llama_plugin_last_error`.

## 3. ABI Versioning

**Problem:** No way to detect plugin/host struct layout incompatibility. Adding fields to `LlamaPluginInfo` or `LlamaFileContext` in the future would cause silent memory corruption.

**Changes:**

Add `uint32_t struct_size` as the first field of both `LlamaPluginInfo` and `LlamaFileContext`. The struct size is the version -- no separate `api_version` field. This is the standard C ABI extensibility pattern (`cbSize`).

```c
typedef struct {
    uint32_t    struct_size;       /* sizeof(LlamaPluginInfo) */
    const char* name;
    const char* version;
} LlamaPluginInfo;

typedef struct {
    uint32_t      struct_size;     /* sizeof(LlamaFileContext) */
    const char*   file_signature;
    uint64_t      inode_addr;
    uint64_t      file_size;
    LlamaReadSeek readseek;
} LlamaFileContext;
```

Host sets `struct_size = sizeof(LlamaFileContext)` before passing to plugins. Plugins set `struct_size = sizeof(LlamaPluginInfo)` during init. Future field additions go at the end; receivers check `struct_size` to know which fields are valid.

## 4. Init Signature

**Problem:** `llama_plugin_init` returns `LlamaPluginInfo` by value (unnecessary copy across ABI boundary). Init failure is signaled by returning a null `name`, which is non-obvious. The `duckdb_connection` is passed as `&dbConn` (double pointer) when it should be passed directly.

**Changes:**

`llama_plugin_init` takes an out-parameter and returns an error code:

```c
// Plugin lifecycle.
// duckdb_handle is a duckdb_connection for DB access during init only.
// Returns 0 on success, negative on failure.
LLAMA_PLUGIN_EXPORT int  llama_plugin_init(void* duckdb_handle, LlamaPluginInfo* info);
LLAMA_PLUGIN_EXPORT void llama_plugin_shutdown(void);
```

Host side in `loadPlugins()`:

```cpp
LlamaPluginInfo info{};
if (initFn(dbConn, &info) < 0) {     // dbConn passed directly, not &dbConn
    std::cerr << "Warning: " << entry.path().filename()
              << " init failed, skipping\n";
    continue;
}
```

Plugin side:

```c
LLAMA_PLUGIN_EXPORT int llama_plugin_init(void* duckdb_handle, LlamaPluginInfo* info) {
    info->struct_size = sizeof(LlamaPluginInfo);
    info->name = "my-plugin";
    info->version = "0.1.0";
    return 0;
}
```

## 5. Seek Return Value

**Problem:** The `wrapReadSeek` bridge returns 0 from `seek()` on success, discarding the actual position. Rust's `Seek::seek` returns the new position. A plugin calling `seek()` that wants the resulting position gets 0 instead.

**Changes:**

Return the actual position from `ReadSeek::seek()`:

```cpp
lrs.seek = [](void* o, size_t pos) -> int64_t {
    try {
        return static_cast<int64_t>(static_cast<ReadSeek*>(o)->seek(pos));
    } catch (...) {
        return -1;
    }
};
```

The `size` callback also gets the same try/catch treatment for consistency (it currently lacks exception handling):

```cpp
lrs.size = [](void* o) -> uint64_t {
    try {
        return static_cast<ReadSeek*>(o)->size();
    } catch (...) {
        return 0;
    }
};
```

The C ABI signature is unchanged: `int64_t (*seek)(void* opaque, size_t pos)`. Absolute position only, no whence support. Rust plugin authors use `SeekFrom::Start(pos)`.

## 6. Read Parameter Order Comment

**Problem:** The C++ `ReadSeek::read(len, buf)` and C ABI `read(opaque, buf, len)` have transposed parameter order, which is a maintenance trap.

**Changes:**

Add a comment in the `wrapReadSeek` bridge noting the transposition:

```cpp
// Note: C++ ReadSeek::read(len, buf) has reversed parameter order
// vs C ABI read(opaque, buf, len). The C ABI order matches Rust's
// Read::read(buf: &mut [u8]) convention.
```

No functional change. The C ABI order is correct (matches Rust); the C++ side is the internal oddball.

## 7. Extract wrapReadSeek

**Problem:** `wrapReadSeek` is duplicated identically in `src/processor.cpp` and `test/test_pluginmanager.cpp`. The copies can drift.

**Changes:**

Extract `wrapReadSeek` to a shared header (e.g., `include/plugin_bridge.h`). Both production code and tests include the same implementation. The seek return value fix and `size` callback exception handling from section 5 apply to the single shared copy.

## 8. Shutdown Cleanup

**Problem:** `shutdown()` is called explicitly in tests before the destructor runs, creating unnecessary noise. The explicit call in `llama.cpp` is correct (deterministic ordering before DB writes).

**Changes:**

- Keep the explicit `Plugins->shutdown()` in `llama.cpp` after `Pool.join()` (now `Plugins.shutdown()` since it's no longer a `unique_ptr`).
- Remove explicit `shutdown()` calls in `test/test_pluginmanager.cpp` where the destructor handles cleanup.

## 9. Error Path Tests

**Problem:** The test stub always returns 0 from `llama_plugin_process`. No tests exercise the error path.

**Changes:**

Add test cases that trigger plugin error returns, verifying:
- The `errmsg` out-parameter is populated and freed correctly via `llama_plugin_free_error`
- Errors are logged to the exception log with the correct plugin name
- Processing continues with remaining plugins after a failure

This may involve a second test stub plugin that returns errors, or a mode flag in the existing stub.

## Revised plugin_api.h

For reference, the complete header after all changes:

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
    LlamaReadSeek readseek;
} LlamaFileContext;

typedef struct {
    uint32_t    struct_size;       /* sizeof(LlamaPluginInfo) */
    const char* name;
    const char* version;
} LlamaPluginInfo;

/* Plugin lifecycle.
   duckdb_handle is a duckdb_connection for DB access during init only.
   Fills in info with plugin metadata. Returns 0 on success, negative on failure. */
LLAMA_PLUGIN_EXPORT int  llama_plugin_init(void* duckdb_handle, LlamaPluginInfo* info);
LLAMA_PLUGIN_EXPORT void llama_plugin_shutdown(void);

/* Per-file processing -- returns 0 on success/skip, negative on error.
   On error, *errmsg is set to a plugin-allocated error string.
   Caller must pass *errmsg to llama_plugin_free_error() when done.
   Called from multiple threads concurrently -- plugins must be thread-safe. */
LLAMA_PLUGIN_EXPORT int  llama_plugin_process(const LlamaFileContext* ctx, const char** errmsg);

/* Free an error string returned via llama_plugin_process. */
LLAMA_PLUGIN_EXPORT void llama_plugin_free_error(const char* errmsg);

#ifdef __cplusplus
}
#endif

#endif /* LLAMA_PLUGIN_API_H */
```
