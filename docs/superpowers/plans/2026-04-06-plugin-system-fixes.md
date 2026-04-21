# Plugin System Fixes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix correctness, API clarity, and maintainability issues identified in the plugin system code review.

**Architecture:** Nine changes to the plugin system, roughly ordered by dependency: ABI header changes first (struct_size, init signature, error reporting), then PluginManager refactoring (remove processFile, change to value member), then bridge extraction, then test updates. Each task produces a compilable, testable state.

**Tech Stack:** C11 (plugin ABI), C++20 (host), Boost.DLL, DuckDB C API, Catch2, Meson

**Spec:** `docs/superpowers/specs/2026-04-06-plugin-system-fixes-design.md`

---

## File Map

| File | Action | Responsibility |
|---|---|---|
| `include/plugin_api.h` | Modify | Add `struct_size` to structs, change init/process/error signatures |
| `include/plugin_bridge.h` | Create | Shared `wrapReadSeek()` implementation |
| `include/pluginmanager.h` | Modify | Remove `processFile()`, add `plugins()`/`empty()`, change `loadPlugins()` signature |
| `src/pluginmanager.cpp` | Modify | Remove `processFile()`, update `loadPlugins()` for new ABI |
| `include/processor.h` | Modify | Change `PluginManager*` to `const PluginManager&` in ProcessorContext |
| `src/processor.cpp` | Modify | Inline plugin dispatch, use `plugin_bridge.h`, add try/catch |
| `include/llama.h` | Modify | Change `unique_ptr<PluginManager>` to `PluginManager` value member |
| `src/llama.cpp` | Modify | Always construct PluginManager, pass as const ref |
| `test/test_plugin_stub.c` | Modify | New ABI: init out-param, process errmsg out-param, free_error, struct_size |
| `test/test_pluginmanager.cpp` | Modify | Use `plugin_bridge.h`, remove explicit shutdown, add error path tests |
| `test/test_plugin_error_stub.c` | Create | Second test stub that always returns errors |
| `test/meson.build` | Modify | Build error stub, pass its directory to tests |

---

### Task 1: Update `plugin_api.h` — ABI Versioning, Init Signature, Error Reporting

This task rewrites the C ABI header. All other tasks depend on it, but nothing compiles until the callers are updated too, so this is a header-only change.

**Files:**
- Modify: `include/plugin_api.h`

**Spec sections:** 2 (Error Reporting), 3 (ABI Versioning), 4 (Init Signature), 6 (Read Parameter Order Comment)

- [ ] **Step 1: Replace `include/plugin_api.h` with the revised header**

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

- [ ] **Step 2: Commit**

```bash
git add include/plugin_api.h
git commit -m "feat(plugin): revise C ABI header -- struct_size, init out-param, errmsg out-param

Addresses spec sections 2-4, 6:
- Add struct_size as first field of LlamaPluginInfo and LlamaFileContext
- Change llama_plugin_init to return int with LlamaPluginInfo* out-param
- Replace llama_plugin_last_error with errmsg out-param on process + free_error
- Add parameter-order comment on LlamaReadSeek.read"
```

---

### Task 2: Extract `wrapReadSeek` to Shared Header

**Files:**
- Create: `include/plugin_bridge.h`
- Modify: `src/processor.cpp:44-66` (remove local `wrapReadSeek`)
- Modify: `test/test_pluginmanager.cpp:12-36` (remove local `wrapReadSeek`)

**Spec sections:** 5 (Seek Return Value), 7 (Extract wrapReadSeek)

- [ ] **Step 1: Create `include/plugin_bridge.h`**

This is a C++ header (guarded by `__cplusplus`) providing the bridge between C++ `ReadSeek` and C `LlamaReadSeek`. It includes the seek return-value fix (return actual position) and size exception handling from spec section 5.

```cpp
#pragma once

#include "plugin_api.h"
#include "readseek.h"

inline LlamaReadSeek wrapReadSeek(ReadSeek* rs) {
  LlamaReadSeek lrs;
  lrs.opaque = static_cast<void*>(rs);

  // Note: C++ ReadSeek::read(len, buf) has reversed parameter order
  // vs C ABI read(opaque, buf, len). The C ABI order matches Rust's
  // Read::read(buf: &mut [u8]) convention.
  lrs.read = [](void* o, uint8_t* buf, size_t len) -> int64_t {
    try {
      return static_cast<int64_t>(static_cast<ReadSeek*>(o)->read(len, buf));
    } catch (...) {
      return -1;
    }
  };

  lrs.seek = [](void* o, size_t pos) -> int64_t {
    try {
      return static_cast<int64_t>(static_cast<ReadSeek*>(o)->seek(pos));
    } catch (...) {
      return -1;
    }
  };

  lrs.size = [](void* o) -> uint64_t {
    try {
      return static_cast<ReadSeek*>(o)->size();
    } catch (...) {
      return 0;
    }
  };

  return lrs;
}
```

- [ ] **Step 2: Update `src/processor.cpp` — replace local `wrapReadSeek` with include**

Remove lines 44-66 (the anonymous-namespace `wrapReadSeek` function) and add `#include "plugin_bridge.h"` to the includes at the top of the file.

The existing includes around line 1-15 should gain:
```cpp
#include "plugin_bridge.h"
```

Delete the entire `wrapReadSeek` function definition in the anonymous namespace (lines 44-66). The call sites at line 199 (`pluginCtx.readseek = wrapReadSeek(&entry.getStream());`) remain unchanged — they now resolve to the inline function from the header.

- [ ] **Step 3: Update `test/test_pluginmanager.cpp` — replace local `wrapReadSeek` with include**

Remove lines 12-36 (the anonymous-namespace `wrapReadSeek` function) and add `#include "plugin_bridge.h"` to the includes. The existing include of `"plugin_api.h"` on line 6 can be removed since `plugin_bridge.h` includes it.

- [ ] **Step 4: Build to verify compilation**

Run: `meson compile -C build`

Expected: Compiles successfully. Tests will fail at link time because the stubs haven't been updated yet — that's fine; we're just checking the header compiles.

- [ ] **Step 5: Commit**

```bash
git add include/plugin_bridge.h src/processor.cpp test/test_pluginmanager.cpp
git commit -m "refactor: extract wrapReadSeek to shared header, fix seek return value

- Move wrapReadSeek from anonymous namespaces in processor.cpp and
  test_pluginmanager.cpp to include/plugin_bridge.h
- seek() now returns the actual position instead of 0
- size() now has try/catch for consistency with read/seek
- Add parameter-order transposition comment"
```

---

### Task 3: Update Test Stubs for New ABI

**Files:**
- Modify: `test/test_plugin_stub.c`
- Create: `test/test_plugin_error_stub.c`
- Modify: `test/meson.build`

**Spec sections:** 2 (Error Reporting), 3 (ABI Versioning), 4 (Init Signature), 9 (Error Path Tests)

- [ ] **Step 1: Rewrite `test/test_plugin_stub.c` for the new ABI**

```c
#include "plugin_api.h"
#include <string.h>
#include <stdlib.h>

LLAMA_PLUGIN_EXPORT int llama_plugin_init(void* duckdb_handle, LlamaPluginInfo* info) {
    (void)duckdb_handle;
    info->struct_size = sizeof(LlamaPluginInfo);
    info->name = "test-plugin";
    info->version = "0.1.0";
    return 0;
}

LLAMA_PLUGIN_EXPORT int llama_plugin_process(const LlamaFileContext* ctx, const char** errmsg) {
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

- [ ] **Step 2: Create `test/test_plugin_error_stub.c`**

A second test plugin that always returns an error from `llama_plugin_process`. Used to test error path handling and multi-plugin continuation.

```c
#include "plugin_api.h"
#include <stdlib.h>
#include <string.h>

LLAMA_PLUGIN_EXPORT int llama_plugin_init(void* duckdb_handle, LlamaPluginInfo* info) {
    (void)duckdb_handle;
    info->struct_size = sizeof(LlamaPluginInfo);
    info->name = "error-plugin";
    info->version = "0.1.0";
    return 0;
}

LLAMA_PLUGIN_EXPORT int llama_plugin_process(const LlamaFileContext* ctx, const char** errmsg) {
    (void)ctx;
    *errmsg = strdup("deliberate test error");
    return -1;
}

LLAMA_PLUGIN_EXPORT void llama_plugin_free_error(const char* errmsg) {
    free((void*)errmsg);
}

LLAMA_PLUGIN_EXPORT void llama_plugin_shutdown(void) {
}
```

- [ ] **Step 3: Update `test/meson.build` — build error stub, pass both directories**

Add the error stub shared library after the existing `test_plugin_stub` block (after line 97):

```meson
test_plugin_error_stub = shared_library('test_plugin_error_stub',
  'test_plugin_error_stub.c',
  include_directories: inc,
  name_prefix: '',
  c_args: ['-std=c11'],
)
```

Also update the `test_exe` `cpp_args` (line 107) to add a second define:

```meson
    cpp_args: ['-DPLUGIN_STUB_DIR="' + meson.current_build_dir() + '"'],
```

This stays the same — both stubs are built into the same build directory, so the existing `PLUGIN_STUB_DIR` already points to both `.dylib`/`.so` files. The test will load all plugins from that directory.

However, this means both stubs load together. For tests that need only the good stub, we need a way to isolate. The simplest approach: tests that need only the success stub create a temp directory and symlink/copy just that one. But actually, looking at the test structure, `loadPlugins()` loads *all* `.dylib`/`.so` files from a directory. So if both stubs are in the same directory, all tests would load both.

**Better approach:** Build the error stub into a subdirectory. Update `test/meson.build`:

```meson
test_plugin_error_stub = shared_library('test_plugin_error_stub',
  'test_plugin_error_stub.c',
  include_directories: inc,
  name_prefix: '',
  c_args: ['-std=c11'],
  build_by_default: true,
)
```

Then add a second compile definition for the error stub location. Since meson puts shared libraries in the same build dir by default, we need to control this. Actually, `meson.current_build_dir()` is the same for both. The simplest approach: the error-path tests load from `PLUGIN_STUB_DIR` (which has both), while existing tests that need only the good stub use a directory with just that one.

**Simplest working approach:** Keep both in the same build dir. Existing tests that call `loadPlugins(PLUGIN_STUB_DIR)` will now load 2 plugins. The good stub returns 0, the error stub returns -1. Tests that previously expected `rc == 0` will fail because the error stub always fails.

**Correct approach:** Create a dedicated subdirectory for the error stub. We can't easily control meson's output dirs per-target on all platforms, so instead use a test fixture that creates a temp dir and symlinks only the needed stubs. But that's fragile.

**Simplest correct approach:** Add a `PLUGIN_ERROR_STUB_PATH` define pointing to the error stub's full file path (not directory). Tests that need the error stub create a temp dir and symlink it in, alongside or instead of the good stub. This is the most flexible.

Update the `cpp_args` line in `test/meson.build`:

```meson
    cpp_args: [
      '-DPLUGIN_STUB_DIR="' + meson.current_build_dir() + '"',
      '-DPLUGIN_ERROR_STUB="' + test_plugin_error_stub.full_path() + '"',
      '-DPLUGIN_STUB="' + test_plugin_stub.full_path() + '"',
    ],
```

The tests can then create temp dirs with specific symlinks to control which plugins load.

- [ ] **Step 4: Commit**

```bash
git add test/test_plugin_stub.c test/test_plugin_error_stub.c test/meson.build
git commit -m "feat(test): update stubs for new plugin ABI, add error stub

- Rewrite test_plugin_stub.c: init out-param, process errmsg, free_error, struct_size
- Add test_plugin_error_stub.c that always returns errors
- Pass PLUGIN_STUB and PLUGIN_ERROR_STUB paths via compile definitions"
```

---

### Task 4: Refactor `PluginManager` — Remove `processFile`, Lifecycle-Only API

**Files:**
- Modify: `include/pluginmanager.h`
- Modify: `src/pluginmanager.cpp`

**Spec sections:** 1 (PluginManager Threading Model), 2 (Error Reporting — `LoadedPlugin` changes), 4 (Init Signature — `loadPlugins` caller side)

- [ ] **Step 1: Rewrite `include/pluginmanager.h`**

```cpp
#pragma once

#include "boost_dll.h"
#include "plugin_api.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <duckdb.h>

struct LoadedPlugin {
  std::string name;
  std::string version;
  boost::dll::shared_library library;
  std::function<int(const LlamaFileContext*, const char**)> process;
  std::function<void(const char*)>                         freeError;
  std::function<void()>                                    shutdown;
};

class PluginManager {
public:
  // Thread-safety contract:
  //   - loadPlugins() and shutdown() are single-threaded (main thread).
  //   - plugins(), empty(), pluginCount() are const and safe to call
  //     concurrently from processing threads.
  //   - The Plugins vector is never mutated while processing threads are active.
  //   Lifecycle: loadPlugins() -> [processing...] -> shutdown()

  PluginManager() = default;
  ~PluginManager();

  PluginManager(const PluginManager&) = delete;
  PluginManager& operator=(const PluginManager&) = delete;

  void loadPlugins(const std::filesystem::path& pluginDir, duckdb_connection dbConn);

  const std::vector<LoadedPlugin>& plugins() const { return Plugins; }
  bool empty() const { return Plugins.empty(); }
  size_t pluginCount() const { return Plugins.size(); }

  void shutdown();

private:
  std::vector<LoadedPlugin> Plugins;

  static constexpr const char* pluginExtension();
};
```

Key changes:
- `loadPlugins()` takes `duckdb_connection dbConn` (value, not reference) — fixes C-2 double-pointer
- `LoadedPlugin`: `process` takes `(const LlamaFileContext*, const char**)`, `lastError` replaced by `freeError`
- `processFile()` removed; `plugins()`, `empty()` added
- Thread-safety contract documented in comment

- [ ] **Step 2: Rewrite `src/pluginmanager.cpp`**

```cpp
#include "pluginmanager.h"

#include <iostream>

constexpr const char* PluginManager::pluginExtension() {
#if defined(__APPLE__)
  return ".dylib";
#elif defined(_WIN32)
  return ".dll";
#else
  return ".so";
#endif
}

PluginManager::~PluginManager() {
  shutdown();
}

void PluginManager::loadPlugins(const std::filesystem::path& pluginDir, duckdb_connection dbConn) {
  namespace fs = std::filesystem;
  const std::string ext = pluginExtension();

  for (const auto& entry : fs::directory_iterator(pluginDir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    if (entry.path().extension().string() != ext) {
      continue;
    }

    try {
      boost::dll::shared_library lib(entry.path().string());

      if (!lib.has("llama_plugin_init") ||
          !lib.has("llama_plugin_process") ||
          !lib.has("llama_plugin_free_error") ||
          !lib.has("llama_plugin_shutdown")) {
        std::cerr << "Warning: " << entry.path().filename()
                  << " missing required symbols, skipping\n";
        continue;
      }

      auto initFn    = lib.get<int(void*, LlamaPluginInfo*)>("llama_plugin_init");
      auto processFn = lib.get<int(const LlamaFileContext*, const char**)>("llama_plugin_process");
      auto freeFn    = lib.get<void(const char*)>("llama_plugin_free_error");
      auto shutdownFn = lib.get<void()>("llama_plugin_shutdown");

      LlamaPluginInfo info{};
      if (initFn(dbConn, &info) < 0) {
        std::cerr << "Warning: " << entry.path().filename()
                  << " init failed, skipping\n";
        continue;
      }

      if (!info.name) {
        std::cerr << "Warning: " << entry.path().filename()
                  << " init returned null name, skipping\n";
        continue;
      }

      LoadedPlugin plugin;
      plugin.name = info.name;
      plugin.version = info.version ? info.version : "unknown";
      plugin.library = std::move(lib);
      plugin.process = processFn;
      plugin.freeError = freeFn;
      plugin.shutdown = shutdownFn;

      std::cerr << "Loaded plugin: " << plugin.name
                << " v" << plugin.version << "\n";

      Plugins.push_back(std::move(plugin));
    }
    catch (const std::exception& e) {
      std::cerr << "Warning: failed to load " << entry.path().filename()
                << ": " << e.what() << "\n";
    }
  }
}

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

Key changes:
- `loadPlugins()` signature: `duckdb_connection dbConn` (value, not `&`)
- `initFn(dbConn, &info)` — passes connection directly, not `&dbConn`
- `initFn` return checked for `< 0`
- Symbol resolution checks for `llama_plugin_free_error` instead of `llama_plugin_last_error`
- Function signatures match new ABI: `int(void*, LlamaPluginInfo*)` for init, `int(const LlamaFileContext*, const char**)` for process
- `processFile()` removed entirely

- [ ] **Step 3: Commit**

```bash
git add include/pluginmanager.h src/pluginmanager.cpp
git commit -m "refactor(plugin): PluginManager as pure lifecycle manager

- Remove processFile(); plugin dispatch moves to Processor
- loadPlugins() takes duckdb_connection by value (fixes double-pointer)
- LoadedPlugin: replace lastError with freeError
- Add plugins()/empty() const accessors
- Document thread-safety contract"
```

---

### Task 5: Wire `const PluginManager&` into `ProcessorContext` and Inline Plugin Dispatch

**Files:**
- Modify: `include/processor.h:53`
- Modify: `src/processor.cpp:69-86` (constructor), `src/processor.cpp:191-208` (plugin dispatch)
- Modify: `src/llama.cpp:95-98` (wiring), `src/llama.cpp:362-371` (loadPlugins)
- Modify: `include/llama.h:59`
- Modify: `test/test_processor.cpp` (~8 callsites that create `ProcessorContext`)
- Modify: `test/test_readseek.cpp` (3 callsites that create `ProcessorContext`)

**Spec sections:** 1 (PluginManager Threading Model — const ref, value member), 5 (seek already fixed in Task 2)

**Note:** The `ProcessorContext` constructor gains a required `const PluginManager&` parameter. Every callsite must be updated. Tests that don't need plugins pass a default-constructed (empty) `PluginManager`.

- [ ] **Step 1: Update `include/processor.h` — change `PluginManager*` to `const PluginManager&`**

Change the `ProcessorContext` struct. The `Plugins` member changes from a nullable pointer to a const reference. Since references can't be null and must be initialized, add it as a constructor parameter.

Replace the `PluginManager* Plugins = nullptr;` line (line 53) and update the constructor signature (lines 30-39):

```cpp
struct ProcessorContext {
  ProcessorContext(
    LlamaDB* db,
    const std::shared_ptr<ProgramHandle>& prog,
    const std::shared_ptr<LlamaRuleEngine> ruleEngine,
    const std::string& exclusionHsetPath,
    const std::string& inclusionHsetPath,
    const MagicsType& sigMagics,
    const PluginManager& plugins,
    const std::shared_ptr<ProgramHandle>& sigProg = nullptr,
    ProgressInfo* progress = nullptr
  );

  // Returns hash flag to be used when initializing a SFHASH_Hasher, based
  // on what's supported by the context's hash sets.
  uint32_t getSupportedHashAlgsFromContext();

  LlamaDB* Db;
  const std::shared_ptr<ProgramHandle> Prog;
  const std::shared_ptr<LlamaRuleEngine> RuleEngine;
  std::unique_ptr<LlamaHashset> ExclusionHashset;
  std::unique_ptr<LlamaHashset> InclusionHashset;
  MagicsType SigMagics;
  std::shared_ptr<ProgramHandle> SigProg;
  ProgressInfo* Progress;
  const PluginManager& Plugins;
};
```

Add `#include "pluginmanager.h"` to the includes (the forward declaration `class PluginManager;` on line 24 becomes an include since we now need the full type for the const reference member).

- [ ] **Step 2: Update `src/processor.cpp` — constructor and plugin dispatch**

Update the `ProcessorContext` constructor (around line 69) to accept and initialize the `Plugins` reference:

```cpp
ProcessorContext::ProcessorContext(LlamaDB* db,
                                   const std::shared_ptr<ProgramHandle>& prog,
                                   const std::shared_ptr<LlamaRuleEngine> ruleEngine,
                                   const std::string& exclusionHsetPath,
                                   const std::string& inclusionHsetPath,
                                   const MagicsType& sigMagics,
                                   const PluginManager& plugins,
                                   const std::shared_ptr<ProgramHandle>& sigProg,
                                   ProgressInfo* progress) :
  Db(db), Prog(prog), RuleEngine(ruleEngine), SigMagics(sigMagics), SigProg(sigProg), Progress(progress), Plugins(plugins) {
  if (!exclusionHsetPath.empty()) {
    ExclusionHashset.reset(new LlamaHashset(exclusionHsetPath.c_str()));
  }

  if (!inclusionHsetPath.empty()) {
    InclusionHashset.reset(new LlamaHashset(inclusionHsetPath.c_str()));
    RuleEngine->addRuleRec(RuleRec{InclusionHashset->getHash(), InclusionHashset->getName()});
  }
}
```

Replace the plugin dispatch block (lines 190-208) with the inlined version from the spec:

```cpp
  // Dispatch to plugins
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

Key differences from old code:
- `Context->Plugins.empty()` instead of `Context->Plugins` (null-pointer check)
- Iterates plugins directly, no `processFile()` call
- `struct_size` set on `pluginCtx`
- Seeks to 0 before each plugin (not just once before the loop)
- Each plugin wrapped in try/catch
- Errors logged independently; all plugins run regardless of failures
- `errmsg` out-parameter + `freeError` replaces `lastError()`

- [ ] **Step 3: Update `include/llama.h` — `PluginManager` as value member**

Replace `std::unique_ptr<PluginManager> Plugins;` (line 59) with:

```cpp
  PluginManager Plugins;
```

- [ ] **Step 4: Update `src/llama.cpp` — always construct, pass as const ref**

Update `Llama::loadPlugins()` (lines 361-371). Since `Plugins` is now a value member (always constructed), just call `loadPlugins` directly:

```cpp
bool Llama::loadPlugins() {
  if (Opts->PluginDir.empty()) {
    return true;
  }
  Plugins.loadPlugins(Opts->PluginDir, DbConn.get());
  if (Plugins.pluginCount() == 0) {
    std::cerr << "Warning: no plugins found in " << Opts->PluginDir << "\n";
  }
  return true;
}
```

Update the `ProcessorContext` construction in `search()` (around line 95). Remove the conditional `Plugins` wiring and pass `Plugins` as a constructor argument:

```cpp
    auto procContext = std::make_shared<ProcessorContext>(&Db, LgProg, RuleEngine, Opts->ExclusionHashset, Opts->InclusionHashset, SigMagics, Plugins, SigProg, &progressInfo);
```

Remove the `if (Plugins)` block that follows (lines 96-98) — no longer needed since `Plugins` is always passed via the constructor.

Update the shutdown call (lines 162-164). Remove the `if` guard:

```cpp
    Plugins.shutdown();
```

- [ ] **Step 5: Update `test/test_processor.cpp` — add `PluginManager` to all `ProcessorContext` callsites**

Add `#include "pluginmanager.h"` to the includes.

Every `ProcessorContext` construction needs a `PluginManager` reference. Create a local `PluginManager` in each scope and pass it. The pattern is:

For the `ProcessorFixture` struct (line 96), add a `PluginManager` member and pass it:
```cpp
  PluginManager Plugins;  // add as member before Proc

  // in makeProcessor():
  auto procContext = std::make_shared<ProcessorContext>(&Db, pHandle, RuleEngine, "", "", MagicsType{}, Plugins);
```

For standalone test cases that use brace-init (lines 165, 179, 196), add a local `PluginManager` and pass it as the 7th argument:
```cpp
  PluginManager plugins;
  ProcessorContext procCtx{
    nullptr,
    nullptr,
    ruleEngine,
    "test/hsets/md5.hset",
    "test/hsets/sha1.hset",
    MagicsType{},
    plugins
  };
```

For `make_shared<ProcessorContext>` calls (lines 223, 270, 295), insert `plugins` after the `MagicsType{}` or `magics` argument:
```cpp
  PluginManager plugins;
  auto procContext = std::make_shared<ProcessorContext>(
    &db, nullptr, ruleEngine, "", "",
    MagicsType{}, plugins
  );
```

For the SigMagics test (line 270) which passes `sigProg` as the 7th arg, it becomes the 8th:
```cpp
  PluginManager plugins;
  auto procContext = std::make_shared<ProcessorContext>(
    &db, nullptr, ruleEngine, "", "", magics, plugins, sigProg
  );
```

- [ ] **Step 6: Update `test/test_readseek.cpp` — add `PluginManager` to all `ProcessorContext` callsites**

Add `#include "pluginmanager.h"` to the includes.

For each of the 3 `make_shared<ProcessorContext>` calls (lines 566, 586, 619), add a local `PluginManager` and pass it:
```cpp
  PluginManager plugins;
  auto ctx = std::make_shared<ProcessorContext>(
    &db, nullptr, ruleEngine, "", "", MagicsType{}, plugins
  );
```

- [ ] **Step 7: Build to verify compilation**

Run: `meson compile -C build`

Expected: Compiles. Tests may not link yet if test stubs haven't been rebuilt.

- [ ] **Step 8: Commit**

```bash
git add include/processor.h src/processor.cpp include/llama.h src/llama.cpp test/test_processor.cpp test/test_readseek.cpp
git commit -m "refactor(plugin): inline dispatch in Processor, const PluginManager& in context

- ProcessorContext holds const PluginManager& (not raw pointer)
- Llama owns PluginManager as value member (not unique_ptr)
- Plugin dispatch inlined in Processor::process() with per-plugin try/catch
- All plugins run independently; errors logged via logException()
- struct_size set on LlamaFileContext before passing to plugins"
```

---

### Task 6: Update Tests — New API, Remove Explicit Shutdown, Error Path Tests

**Files:**
- Modify: `test/test_pluginmanager.cpp`

**Spec sections:** 8 (Shutdown Cleanup), 9 (Error Path Tests)

- [ ] **Step 1: Rewrite `test/test_pluginmanager.cpp`**

```cpp
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
```

Key changes:
- `#include "plugin_bridge.h"` replaces local `wrapReadSeek` and `"plugin_api.h"`
- No explicit `mgr.shutdown()` calls — destructors handle it (spec section 8)
- `LlamaFileContext` initialized with `{}` and `struct_size` set (spec section 3)
- Direct plugin iteration via `mgr.plugins()` instead of `processFile()`
- New `testPluginErrorReporting` — verifies errmsg is populated and freed correctly
- New `testPluginContinuesAfterError` — verifies both plugins run, errors collected independently
- `testProcessorWithPlugin` passes `plugins` to `ProcessorContext` constructor (not set via pointer)
- Each test isolates plugins via temp dir + file copy to avoid cross-contamination

- [ ] **Step 2: Build and run tests**

Run: `meson compile -C build && meson test -C build --print-errorlogs`

Expected: All tests pass.

- [ ] **Step 3: Commit**

```bash
git add test/test_pluginmanager.cpp
git commit -m "test(plugin): update tests for new ABI, add error path coverage

- Use plugin_bridge.h instead of local wrapReadSeek
- Remove explicit shutdown() calls (destructors handle cleanup)
- Set struct_size on LlamaFileContext in all tests
- Add testPluginErrorReporting: errmsg populated and freed
- Add testPluginContinuesAfterError: all plugins run despite failures
- Isolate tests via temp dirs with per-test plugin selection"
```

---

### Task 7: Final Verification

**Files:** None (verification only)

- [ ] **Step 1: Full clean build**

Run: `meson setup build --wipe && meson compile -C build`

Expected: Clean compilation with no warnings related to plugin code.

- [ ] **Step 2: Run all tests**

Run: `meson test -C build --print-errorlogs`

Expected: All tests pass, including the new error path tests.

- [ ] **Step 3: Verify no regressions in non-plugin tests**

Scan the test output to confirm that existing tests (not plugin-related) still pass. The `ProcessorContext` constructor change affects `test_processor.cpp` and any other test that creates a `ProcessorContext` — verify those compile and pass.

- [ ] **Step 4: Spot-check the spec coverage**

Verify each spec section is addressed:
1. PluginManager Threading Model — Task 4 (lifecycle-only API), Task 5 (const ref in context)
2. Error Reporting Redesign — Task 1 (ABI), Task 4 (LoadedPlugin), Task 5 (dispatch)
3. ABI Versioning — Task 1 (struct_size fields)
4. Init Signature — Task 1 (ABI), Task 4 (loadPlugins caller)
5. Seek Return Value — Task 2 (plugin_bridge.h)
6. Read Parameter Order Comment — Task 1 (ABI header), Task 2 (bridge comment)
7. Extract wrapReadSeek — Task 2
8. Shutdown Cleanup — Task 6 (remove explicit shutdown in tests)
9. Error Path Tests — Task 3 (error stub), Task 6 (error tests)
