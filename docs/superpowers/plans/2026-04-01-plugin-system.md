# Plugin System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add runtime-loadable shared library plugin support so llama can optionally dispatch file-format-specific artifact parsers implemented in external shared libraries.

**Architecture:** A C ABI header (`plugin_api.h`) defines the contract. A `PluginManager` class uses Boost.DLL to discover and load plugins from a user-specified directory. Plugins are called after file signature detection in `Processor::process()`. Plugins get a DuckDB connection at init and a file context (signature, inode addr, file size, ReadSeek vtable) per file.

**Tech Stack:** C++20, Boost.DLL (header-only), DuckDB C API, Meson build system, Catch2 tests

---

## File Structure

| Action | File | Responsibility |
|--------|------|----------------|
| Create | `include/plugin_api.h` | C ABI contract header (pure C, shared with plugin authors) |
| Create | `include/pluginmanager.h` | PluginManager class declaration |
| Create | `src/pluginmanager.cpp` | Plugin loading, dispatch, lifecycle |
| Create | `test/test_pluginmanager.cpp` | Unit tests for PluginManager |
| Create | `test/test_plugin_stub.c` | Minimal C test plugin shared library |
| Modify | `include/options.h:8-21` | Add `PluginDir` field |
| Modify | `src/cli.cpp:19-64` | Add `--plugin-dir` option and validation |
| Modify | `test/test_cli.cpp` | Add CLI tests for `--plugin-dir` |
| Modify | `include/processor.h:28-52` | Add `PluginManager*` to ProcessorContext |
| Modify | `src/processor.cpp:125-172` | Call plugins after file sig detection |
| Modify | `include/llama.h:22-57` | Add PluginManager member, `loadPlugins()` method |
| Modify | `src/llama.cpp:354-380` | Load plugins during init |
| Modify | `src/meson.build:1-47` | Add `pluginmanager.cpp` to sources |
| Modify | `test/meson.build:1-135` | Add test plugin shared library + test source |

---

### Task 1: CLI — Add `--plugin-dir` option

**Files:**
- Modify: `include/options.h:8-21`
- Modify: `src/cli.cpp:19-64`
- Modify: `src/cli.cpp:139-158`
- Test: `test/test_cli.cpp`

- [ ] **Step 1: Write failing test for `--plugin-dir` parsing**

Add to `test/test_cli.cpp`:

```cpp
TEST_CASE("testCLIPluginDir") {
  const char* args[] = {"llama", "--plugin-dir", "/tmp/plugins", "output", "input.E01"};
  Cli cli;
  auto opts = cli.parse(5, args);
  REQUIRE("search" == opts->Command);
  REQUIRE("/tmp/plugins" == opts->PluginDir);
}

TEST_CASE("testCLIPluginDirEmpty") {
  const char* args[] = {"llama", "output", "input.E01"};
  Cli cli;
  auto opts = cli.parse(3, args);
  REQUIRE(opts->PluginDir.empty());
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd builddir && meson compile && ./llama_test "testCLIPluginDir"`
Expected: Compilation error — `Options` has no member `PluginDir`

- [ ] **Step 3: Add `PluginDir` to Options struct**

In `include/options.h`, add after the `SignaturesPath` line:

```cpp
  std::string PluginDir;
```

- [ ] **Step 4: Add `--plugin-dir` option to CLI**

In `src/cli.cpp`, add to the `configOpts` block (after the `"inclusion-hashset"` entry, before the closing semicolon):

```cpp
      ("plugin-dir",
        po::value<std::string>(&Opts->PluginDir)
        ->default_value("")
        ->value_name("PLUGIN_DIR"),
        "Directory containing plugin shared libraries")
```

- [ ] **Step 5: Add validation for `--plugin-dir`**

In `src/cli.cpp` `validateOpts()`, add after the duplicate evidence filename check:

```cpp
  if (!Opts->PluginDir.empty()) {
    THROW_IF(!std::filesystem::exists(Opts->PluginDir), "Plugin directory " + Opts->PluginDir + " not found.");
    THROW_IF(!std::filesystem::is_directory(Opts->PluginDir), "Plugin directory " + Opts->PluginDir + " is not a directory.");
  }
```

- [ ] **Step 6: Run tests to verify they pass**

Run: `cd builddir && meson compile && ./llama_test "testCLIPluginDir,testCLIPluginDirEmpty"`
Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add include/options.h src/cli.cpp test/test_cli.cpp
git commit -m "feat: add --plugin-dir CLI option"
```

---

### Task 2: Plugin API header

**Files:**
- Create: `include/plugin_api.h`

- [ ] **Step 1: Create the C ABI header**

Create `include/plugin_api.h`:

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
    const char*   file_signature;   /* NULL if no signature matched */
    uint64_t      inode_addr;
    uint64_t      file_size;
    LlamaReadSeek readseek;
} LlamaFileContext;

typedef struct {
    const char* name;
    const char* version;
} LlamaPluginInfo;

/* Plugin lifecycle */
LLAMA_PLUGIN_EXPORT LlamaPluginInfo llama_plugin_init(void* duckdb_handle);
LLAMA_PLUGIN_EXPORT void            llama_plugin_shutdown(void);

/* Per-file processing — returns 0 on success/skip, negative on error */
LLAMA_PLUGIN_EXPORT int             llama_plugin_process(const LlamaFileContext* ctx);

/* Error reporting — returns thread-local string describing last error */
LLAMA_PLUGIN_EXPORT const char*     llama_plugin_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* LLAMA_PLUGIN_API_H */
```

- [ ] **Step 2: Verify header compiles as C and C++**

Run: `gcc -fsyntax-only -std=c11 include/plugin_api.h && g++ -fsyntax-only -std=c++20 include/plugin_api.h && echo "OK"`
Expected: OK (no errors)

- [ ] **Step 3: Commit**

```bash
git add include/plugin_api.h
git commit -m "feat: add plugin C ABI contract header"
```

---

### Task 3: Test stub plugin (C shared library)

**Files:**
- Create: `test/test_plugin_stub.c`
- Modify: `test/meson.build`

- [ ] **Step 1: Create the test stub plugin**

Create `test/test_plugin_stub.c`:

```c
#include "plugin_api.h"
#include <string.h>

static _Thread_local const char* last_err = NULL;

LLAMA_PLUGIN_EXPORT LlamaPluginInfo llama_plugin_init(void* duckdb_handle) {
    (void)duckdb_handle;
    LlamaPluginInfo info;
    info.name = "test-plugin";
    info.version = "0.1.0";
    return info;
}

LLAMA_PLUGIN_EXPORT int llama_plugin_process(const LlamaFileContext* ctx) {
    if (ctx->file_signature && strcmp(ctx->file_signature, "SQLite Database") == 0) {
        /* Read first 16 bytes to verify ReadSeek works */
        uint8_t buf[16];
        int64_t n = ctx->readseek.read(ctx->readseek.opaque, buf, 16);
        if (n < 0) {
            last_err = "read failed";
            return -1;
        }
        return 0;
    }
    return 0;
}

LLAMA_PLUGIN_EXPORT const char* llama_plugin_last_error(void) {
    return last_err;
}

LLAMA_PLUGIN_EXPORT void llama_plugin_shutdown(void) {
    last_err = NULL;
}
```

- [ ] **Step 2: Add shared library target to test meson.build**

In `test/meson.build`, add before the `if catch2_dep.found()` block:

```meson
# Test plugin stub (always built if tests are enabled)
test_plugin_stub = shared_library('test_plugin_stub',
  'test_plugin_stub.c',
  include_directories: inc,
  name_prefix: '',
  c_args: ['-std=c11'],
)
```

- [ ] **Step 3: Build and verify the shared library compiles**

Run: `cd builddir && meson compile`
Expected: Compiles without error. Produces `test/test_plugin_stub.so` (or `.dylib` on macOS).

- [ ] **Step 4: Commit**

```bash
git add test/test_plugin_stub.c test/meson.build
git commit -m "feat: add test stub plugin shared library"
```

---

### Task 4: PluginManager — loading and lifecycle

**Files:**
- Create: `include/pluginmanager.h`
- Create: `src/pluginmanager.cpp`
- Modify: `src/meson.build:1-47`
- Test: `test/test_pluginmanager.cpp`
- Modify: `test/meson.build`

- [ ] **Step 1: Write failing test for plugin loading**

Create `test/test_pluginmanager.cpp`:

```cpp
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
```

- [ ] **Step 2: Add test source and build**

In `test/meson.build`, add `'test_pluginmanager.cpp'` to the `test_sources` list.

Run: `cd builddir && meson compile`
Expected: Compilation error — `pluginmanager.h` not found

- [ ] **Step 3: Create PluginManager header**

Create `include/pluginmanager.h`:

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
  std::function<int(const LlamaFileContext*)>  process;
  std::function<const char*()>                 lastError;
  std::function<void()>                        shutdown;
};

class PluginManager {
public:
  PluginManager() = default;
  ~PluginManager();

  PluginManager(const PluginManager&) = delete;
  PluginManager& operator=(const PluginManager&) = delete;

  void loadPlugins(const std::filesystem::path& pluginDir, duckdb_connection& dbConn);

  // Returns 0 if all plugins succeeded/skipped, negative on first error.
  // On error, errorPlugin and errorMessage are set.
  int processFile(const LlamaFileContext& ctx,
                  std::string& errorPlugin,
                  std::string& errorMessage);

  size_t pluginCount() const { return Plugins.size(); }

  void shutdown();

private:
  std::vector<LoadedPlugin> Plugins;

  static constexpr const char* pluginExtension();
};
```

- [ ] **Step 4: Create PluginManager implementation**

Create `src/pluginmanager.cpp`:

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

void PluginManager::loadPlugins(const std::filesystem::path& pluginDir, duckdb_connection& dbConn) {
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
          !lib.has("llama_plugin_last_error") ||
          !lib.has("llama_plugin_shutdown")) {
        std::cerr << "Warning: " << entry.path().filename()
                  << " missing required symbols, skipping\n";
        continue;
      }

      auto initFn = lib.get<LlamaPluginInfo(void*)>("llama_plugin_init");
      auto processFn = lib.get<int(const LlamaFileContext*)>("llama_plugin_process");
      auto lastErrorFn = lib.get<const char*()>("llama_plugin_last_error");
      auto shutdownFn = lib.get<void()>("llama_plugin_shutdown");

      LlamaPluginInfo info = initFn(&dbConn);
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
      plugin.lastError = lastErrorFn;
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

int PluginManager::processFile(const LlamaFileContext& ctx,
                                std::string& errorPlugin,
                                std::string& errorMessage) {
  for (auto& plugin : Plugins) {
    int rc = plugin.process(&ctx);
    if (rc < 0) {
      errorPlugin = plugin.name;
      const char* err = plugin.lastError();
      errorMessage = err ? err : "unknown error";
      return rc;
    }
  }
  return 0;
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

- [ ] **Step 5: Add `pluginmanager.cpp` to build**

In `src/meson.build`, add `'pluginmanager.cpp'` to the `llama_sources` list (alphabetical order, after `'pooloutputhandler.cpp'`).

In `test/meson.build`, add `'../src/pluginmanager.cpp'` to the `llama_common_sources` list (same position).

- [ ] **Step 6: Run tests to verify they pass**

Run: `cd builddir && meson compile && ./llama_test "testPluginManagerLoad"`
Expected: Both tests PASS

- [ ] **Step 7: Commit**

```bash
git add include/pluginmanager.h src/pluginmanager.cpp src/meson.build test/test_pluginmanager.cpp test/meson.build
git commit -m "feat: add PluginManager with loading and lifecycle"
```

---

### Task 5: PluginManager — loading the test stub plugin

**Files:**
- Modify: `test/test_pluginmanager.cpp`

- [ ] **Step 1: Write failing test that loads the test stub plugin**

Add to `test/test_pluginmanager.cpp`:

```cpp
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
```

- [ ] **Step 2: Pass stub plugin directory as a compile definition**

In `test/meson.build`, modify the `test_exe` target to add a cpp_args definition that provides the path to the directory containing the test stub plugin. Add after the `dependencies` line:

```meson
    cpp_args: ['-DPLUGIN_STUB_DIR="' + test_plugin_stub.outdir() + '"'],
```

Note: `test_plugin_stub.outdir()` returns the build directory where the shared library is produced. If Meson doesn't support `.outdir()`, use `meson.current_build_dir()` instead since the stub is built in the test directory.

- [ ] **Step 3: Run test to verify it passes**

Run: `cd builddir && meson compile && ./llama_test "testPluginManagerLoadStub"`
Expected: PASS — plugin loads, pluginCount() == 1

- [ ] **Step 4: Commit**

```bash
git add test/test_pluginmanager.cpp test/meson.build
git commit -m "test: verify PluginManager loads test stub plugin"
```

---

### Task 6: PluginManager — processFile with ReadSeek bridge

**Files:**
- Modify: `test/test_pluginmanager.cpp`

- [ ] **Step 1: Write failing test for processFile dispatch**

Add to `test/test_pluginmanager.cpp`:

```cpp
#include "readseek_impl.h"
#include "plugin_api.h"

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
```

- [ ] **Step 2: Run tests to verify they pass**

Run: `cd builddir && meson compile && ./llama_test "testPluginManagerProcess"`
Expected: Both tests PASS

- [ ] **Step 3: Commit**

```bash
git add test/test_pluginmanager.cpp
git commit -m "test: verify plugin processFile dispatch and ReadSeek bridge"
```

---

### Task 7: Integrate PluginManager into ProcessorContext and Processor

**Files:**
- Modify: `include/processor.h:28-52`
- Modify: `src/processor.cpp:125-172`

- [ ] **Step 1: Add PluginManager to ProcessorContext**

In `include/processor.h`, add forward declaration near the top (after `class ProgressInfo;`):

```cpp
class PluginManager;
```

Add to `ProcessorContext` struct, after `ProgressInfo* Progress;`:

```cpp
  PluginManager* Plugins = nullptr;
```

- [ ] **Step 2: Add wrapReadSeek helper and plugin dispatch to Processor::process**

In `src/processor.cpp`, add include at the top (after the other includes):

```cpp
#include "pluginmanager.h"
#include "plugin_api.h"
```

Add the `wrapReadSeek` helper function in the anonymous namespace (after the `hashFile` function):

```cpp
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
```

In `Processor::process()`, add plugin dispatch after the file signatures block (after line 162, before the search block):

```cpp
  // Dispatch to plugins
  if (Context->Plugins) {
    try {
      entry.getStream().seek(0);
      const char* sigName = sigResults.empty() ? nullptr : sigResults[0]->Name.c_str();
      LlamaFileContext pluginCtx;
      pluginCtx.file_signature = sigName;
      pluginCtx.inode_addr = entry.Addr;
      pluginCtx.file_size = entry.FileSize;
      pluginCtx.readseek = wrapReadSeek(&entry.getStream());

      std::string errorPlugin, errorMessage;
      if (Context->Plugins->processFile(pluginCtx, errorPlugin, errorMessage) < 0) {
        logException(entry, ("plugin:" + errorPlugin).c_str(), errorMessage.c_str());
      }
    } catch (const EvidenceIOError& e) {
      logException(entry, "plugin", e.what());
    }
  }
```

Note: `sigResults` is currently scoped inside the file signatures block. The `sigResults` variable must be moved to be accessible to both the signatures block and the plugin dispatch block. Move the `std::vector<MagicPtr> sigResults;` declaration to before the signatures block (before line 151), and remove the inner scope braces so `sigResults` is still in scope for the plugin dispatch.

Specifically, replace lines 150-162:

```cpp
  // Detect file signatures
  std::vector<MagicPtr> sigResults;
  {
    try {
      entry.getStream().seek(0);
      SigAnalyzer.getSignatures(entry.getStream(), sigResults);
      for (const auto& sig : sigResults) {
        FileSigs->add(FileSigResult{HashRecord.SHA256, sig->Id});
      }
    } catch (const EvidenceIOError& e) {
      logException(entry, "signature", e.what());
    }
  }
```

- [ ] **Step 3: Verify existing tests still pass**

Run: `cd builddir && meson compile && ./llama_test`
Expected: All existing tests PASS (plugins pointer is nullptr by default, so plugin block is skipped)

- [ ] **Step 4: Commit**

```bash
git add include/processor.h src/processor.cpp
git commit -m "feat: integrate PluginManager into Processor pipeline"
```

---

### Task 8: Wire PluginManager into Llama initialization

**Files:**
- Modify: `include/llama.h:22-57`
- Modify: `src/llama.cpp:73-178` (search method)
- Modify: `src/llama.cpp:354-380` (init method)

- [ ] **Step 1: Add PluginManager member to Llama class**

In `include/llama.h`, add include (after `#include "options.h"`):

```cpp
#include "pluginmanager.h"
```

Add to Llama's private members (after `LlamaDBConnection DbConn;`):

```cpp
  std::unique_ptr<PluginManager> Plugins;
```

Add private method declaration (after `bool loadSignatures();`):

```cpp
  bool loadPlugins();
```

- [ ] **Step 2: Implement loadPlugins**

In `src/llama.cpp`, add the `loadPlugins` method (after `loadSignatures()`):

```cpp
bool Llama::loadPlugins() {
  if (Opts->PluginDir.empty()) {
    return true;
  }
  Plugins = std::make_unique<PluginManager>();
  Plugins->loadPlugins(Opts->PluginDir, DbConn.get());
  if (Plugins->pluginCount() == 0) {
    std::cerr << "Warning: no plugins found in " << Opts->PluginDir << "\n";
  }
  return true;
}
```

- [ ] **Step 3: Call loadPlugins during init**

In `Llama::init()`, add a future for plugin loading alongside the existing futures (after the `sigs` future):

```cpp
  auto plugins = make_future(Pool, [this]() {
    return loadPlugins();
  });
```

Note: `loadPlugins` depends on `dbInit` completing first (it needs DbConn). Since `init()` currently runs all futures in parallel and waits at the end, and `loadPlugins` uses `DbConn` (the main connection, not a new one), it must run after `db` completes. Restructure: call `loadPlugins()` synchronously after all futures resolve, before the return.

Replace the return line:

```cpp
  bool ok = readPats.get() && open.get() && db.get() && rules.get() && sigs.get();
  if (ok) {
    ok = loadPlugins();
  }
  return ok;
```

- [ ] **Step 4: Pass PluginManager to ProcessorContext**

In `Llama::search()`, after constructing `procContext` (line 95), add:

```cpp
    if (Plugins) {
      procContext->Plugins = Plugins.get();
    }
```

- [ ] **Step 5: Call shutdown on Llama::search() cleanup**

In `Llama::search()`, after `Pool.join()` (line 157) and before `RuleEngine->writeRulesToDb(DbConn)`:

```cpp
    if (Plugins) {
      Plugins->shutdown();
    }
```

- [ ] **Step 6: Verify the build compiles**

Run: `cd builddir && meson compile && ./llama_test`
Expected: All tests PASS

- [ ] **Step 7: Commit**

```bash
git add include/llama.h src/llama.cpp
git commit -m "feat: wire PluginManager into Llama init and search lifecycle"
```

---

### Task 9: Integration test — end-to-end plugin dispatch

**Files:**
- Modify: `test/test_pluginmanager.cpp`

- [ ] **Step 1: Write integration test that loads stub plugin and processes a file through Processor**

Add to `test/test_pluginmanager.cpp`:

```cpp
#include "processor.h"
#include "entry.h"

TEST_CASE("testProcessorWithPlugin") {
  auto pluginDir = std::filesystem::path(PLUGIN_STUB_DIR);

  LlamaDB db;
  LlamaDBConnection conn(db);

  // Create tables needed by Processor
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
  auto ruleEngine = std::make_shared<LlamaRuleEngine>();
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
  // The test stub plugin returns 0 for all files, so no exceptions expected.
  plugins.shutdown();
}
```

Note: This test needs the additional includes already added in Task 6. Add `#include "duckhash.h"`, `#include "llamabatch.h"`, `#include "duckexception.h"`, `#include "ducksig.h"` if not already present.

- [ ] **Step 2: Run the integration test**

Run: `cd builddir && meson compile && ./llama_test "testProcessorWithPlugin"`
Expected: PASS

- [ ] **Step 3: Commit**

```bash
git add test/test_pluginmanager.cpp
git commit -m "test: integration test for Processor with plugin dispatch"
```

---

### Task 10: Update help text and final cleanup

**Files:**
- Modify: `src/cli.cpp:89-93`

- [ ] **Step 1: Update help text to mention plugins**

In `src/cli.cpp`, update the usage line in `printHelp`:

```cpp
  out << "\nUsage: llama [OPTIONS] OUTPUT_DIRECTORY INPUT_FILE [INPUT_FILE ...]\n"
      << "\nPlugins: Use --plugin-dir to load artifact parser plugins from a directory.\n"
      << All << std::endl;
```

- [ ] **Step 2: Run full test suite**

Run: `cd builddir && meson compile && ./llama_test`
Expected: All tests PASS

- [ ] **Step 3: Commit**

```bash
git add src/cli.cpp
git commit -m "docs: add plugin usage hint to CLI help text"
```
