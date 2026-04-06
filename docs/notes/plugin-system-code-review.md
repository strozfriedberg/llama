# Plugin System Code Review

**Date:** 2026-04-02
**Commits reviewed:** c02d94d..53b0a5c (10 commits, 518 insertions across 16 files)
**Branch:** readseek-contract-hardening

## Executive Summary

The plugin system is a well-structured, incremental addition that cleanly integrates runtime-loadable shared library plugins into the llama processing pipeline. The architecture -- C ABI header, Boost.DLL loading, per-file dispatch after signature detection -- is sound and follows the plan closely. However, there are two correctness issues that need attention before production use: a thread-safety problem with `PluginManager::processFile()` being called from multiple threads while iterating a shared `Plugins` vector, and a `duckdb_connection` double-pointer issue at the C ABI boundary. The remaining findings are moderate or minor.

---

## Critical

### C-1. Thread safety: `processFile()` called concurrently on shared `PluginManager`

**File:** `src/pluginmanager.cpp:75-88`, `src/processor.cpp:191-208`

`PluginManager::processFile()` iterates over `Plugins` (a `std::vector<LoadedPlugin>`) and calls each plugin's `process` and `lastError` std::function objects. The `ProcessorContext::Plugins` pointer is shared across all `Processor` clones (set once in `llama.cpp:96-98`, then shared via `ProcessorContext` which is a `shared_ptr`). Each `Processor` clone runs on a different thread via `FileScheduler::dispatchIfReady()` posting to the thread pool (`filescheduler.cpp:117`).

The `Plugins` vector itself is not mutated during processing (it is only populated during `loadPlugins()` and cleared during `shutdown()`), so iterating it concurrently is safe. The `std::function` objects are read-only callable wrappers around raw function pointers, so calling them concurrently is also safe. **The real thread-safety contract is pushed to the plugins themselves** -- each plugin's `llama_plugin_process()` must be thread-safe, which is documented in `plugin_api.h:43`.

However, `processFile()` also writes to `errorPlugin` and `errorMessage` (output parameters passed by reference from `Processor::process()`), which are local variables on each thread's stack, so that is fine.

**Revised assessment:** On closer analysis, the concurrent read access to the `Plugins` vector is safe because it is never mutated during the processing phase. The `shutdown()` call happens after `Pool.join()` in `llama.cpp:160-164`, which ensures all processing threads have completed. **This is actually correct, but the safety relies on an implicit ordering guarantee that is not documented.** If someone were to call `shutdown()` or `loadPlugins()` while processing is active, it would be a data race.

**Suggested fix:** Add a comment to `PluginManager` documenting the thread-safety contract: the `Plugins` vector must not be mutated while `processFile()` may be called from other threads. Alternatively, mark `processFile()` as `const` (it does not modify any `PluginManager` state) to make the contract clearer at the type level.

### C-2. `duckdb_connection` double-pointer passed to plugin init

**File:** `src/pluginmanager.cpp:48`, `include/pluginmanager.h:30`

The `loadPlugins` signature takes `duckdb_connection& dbConn`. In DuckDB's C API, `duckdb_connection` is already a pointer type:

```c
typedef struct _duckdb_connection { void *internal_ptr; } * duckdb_connection;
```

So `duckdb_connection& dbConn` is a reference to a pointer. When `initFn(&dbConn)` is called on line 48, the `&` takes the address of the pointer, producing a `duckdb_connection*` -- effectively a `struct _duckdb_connection**` (double pointer). This double pointer is passed to the plugin as `void* duckdb_handle`.

The plugin API comment says `duckdb_handle is a duckdb_connection*` (plugin_api.h:38), which is consistent with the code -- the plugin receives a `duckdb_connection*`. If a plugin author dereferences this to get a `duckdb_connection`, they will get the correct single pointer value.

**However**, the API comment and documentation say "duckdb_connection* for DB access during init only," which means a plugin would do:

```c
duckdb_connection conn = *(duckdb_connection*)duckdb_handle;
```

This is correct but confusing. A more natural API would pass the `duckdb_connection` directly (as `void*`, since it is already a pointer):

```c
duckdb_connection conn = (duckdb_connection)duckdb_handle;
```

**Suggested fix:** Change line 48 to `initFn(dbConn)` (passing the pointer value directly, not a pointer to the pointer). Update the API comment to say `duckdb_handle is a duckdb_connection` (not `duckdb_connection*`). This avoids the double-indirection confusion and aligns with how C APIs typically pass opaque handles.

---

## Important

### I-1. `lastError()` race between `process()` and `lastError()` across threads

**File:** `src/pluginmanager.cpp:79-83`, `test/test_plugin_stub.c:4`

The error reporting pattern has a TOCTOU race. When `processFile()` detects `rc < 0`, it calls `plugin.lastError()` on the next line. The test stub plugin uses `_Thread_local` for `last_err`, which is correct per-thread. But the `lastError()` function pointer is a single `std::function` in `LoadedPlugin` that resolves to the same C function symbol.

If two threads call the same plugin's `process()` concurrently and both fail, each thread's call to `lastError()` will correctly read its own thread-local variable because `_Thread_local` in the stub gives per-thread storage. **The contract requires plugins to use thread-local storage for error reporting**, which is documented in the API header (`plugin_api.h:47`).

This is fine as long as plugins follow the contract. The concern is that the API header says "thread-local string" but does not explain what happens if a plugin does NOT use thread-local storage (the error message would be a data race). Consider adding a stronger note in the API header or in plugin author documentation.

**Suggested fix:** Expand the `llama_plugin_last_error` documentation to explicitly state: "Plugins MUST use thread-local storage for the returned string. Failure to do so will cause data races."

### I-2. Plugin exceptions not caught in `processFile()`

**File:** `src/pluginmanager.cpp:75-88`

The `processFile()` method calls `plugin.process(&ctx)` but does not wrap the call in a try/catch. If a plugin is implemented in C++ (rather than C) and throws an exception through the C ABI boundary, this is undefined behavior. While the C function pointer signature discourages C++ exceptions, a misbehaving plugin could cause a crash.

The `wrapReadSeek` functions in `processor.cpp:44-66` correctly catch exceptions from the ReadSeek callbacks (added in commit 53b0a5c), demonstrating awareness of this class of problem. But the same protection is not applied to the plugin function calls themselves.

**Suggested fix:** Wrap `plugin.process(&ctx)` in a try/catch block:

```cpp
int rc;
try {
  rc = plugin.process(&ctx);
} catch (const std::exception& e) {
  errorPlugin = plugin.name;
  errorMessage = std::string("plugin threw exception: ") + e.what();
  return -1;
} catch (...) {
  errorPlugin = plugin.name;
  errorMessage = "plugin threw unknown exception";
  return -1;
}
```

### I-3. Plugin dispatch stops at first error -- no way to continue with remaining plugins

**File:** `src/pluginmanager.cpp:80-84`

When a plugin returns a negative error code, `processFile()` immediately returns, skipping all remaining plugins. This means if Plugin A fails on a file, Plugin B never sees it. The current behavior is documented ("negative on first error"), but it may not be the desired behavior in production. A failing image format parser should not prevent an unrelated text parser from running.

**Suggested fix:** Consider adding a mode (or changing the default) where all plugins are called and errors are collected. At minimum, document this behavior prominently so users understand that plugin ordering in the directory affects which plugins process which files on error.

### I-4. No ABI version field in `LlamaPluginInfo`

**File:** `include/plugin_api.h:31-34`

The `LlamaPluginInfo` struct has `name` and `version` but no `api_version` field. If the plugin API changes in the future (e.g., adding fields to `LlamaFileContext` or changing function signatures), there is no way for the host to detect that a plugin was compiled against an incompatible API version. The result would be subtle memory corruption or crashes.

**Suggested fix:** Add a `uint32_t api_version` field to `LlamaPluginInfo` and define `LLAMA_PLUGIN_API_VERSION 1` as a constant. Check the returned version in `loadPlugins()` before using the plugin.

### I-5. `LlamaPluginInfo` returned by value across C ABI boundary

**File:** `include/plugin_api.h:39`, `src/pluginmanager.cpp:43,48`

The `llama_plugin_init` function returns `LlamaPluginInfo` by value. This struct contains two `const char*` pointers. Returning a struct by value across a C ABI boundary works correctly for simple POD structs on all major platforms (it is well-defined in the C ABI), so this is technically fine.

However, the `boost::dll::shared_library::get<>` template on line 43 resolves the symbol with C++ name mangling expectations. Because the function is declared `extern "C"` in the header and is compiled as C in the stub, this works -- `boost::dll::shared_library::get` uses `dlsym()` which only looks at the C-level symbol name. The function type signature `LlamaPluginInfo(void*)` in the template parameter is only used for the cast; it does not affect symbol lookup. This is correct.

**No action needed** -- this is a documentation note, not a fix.

---

## Minor

### M-1. `wrapReadSeek` duplicated three times

**File:** `src/processor.cpp:44-66`, `test/test_pluginmanager.cpp:13-35`, plan Task 6 and Task 7

The `wrapReadSeek` function is implemented identically in both `processor.cpp` (anonymous namespace) and `test_pluginmanager.cpp` (anonymous namespace). The test version was added in Task 6 before the Processor integration in Task 7, but it was never removed or consolidated afterward.

The two copies are now slightly different: `processor.cpp` has exception handling in the lambdas (added in commit 53b0a5c), while the test copy also has the same exception handling. So they are in sync at present, but having two copies means they can drift.

**Suggested fix:** Extract `wrapReadSeek` into a shared header (e.g., `include/plugin_bridge.h` or into `plugin_api.h` guarded by `__cplusplus`) so both the production code and tests use the same implementation.

### M-2. `shutdown()` called in destructor and also manually

**File:** `src/pluginmanager.cpp:15-17,90-100`, `src/llama.cpp:162-164`, `test/test_pluginmanager.cpp:67,92,115,157`

The `PluginManager` destructor calls `shutdown()`, and `shutdown()` clears the `Plugins` vector, making it safe to call multiple times. However, the tests and `Llama::search()` also call `shutdown()` explicitly before the destructor runs. This double-shutdown pattern works but is unnecessary noise. The explicit `shutdown()` in `llama.cpp` is called after `Pool.join()` but before the `PluginManager` is destroyed (it lives as a member of `Llama`). The timing is correct -- shutdown happens before DB writes in `writeDB()`.

**Suggested fix:** The explicit call in `llama.cpp:162-164` is valuable for deterministic ordering (shutdown plugins before writing reports). Keep it, but remove the explicit `shutdown()` calls in tests where they serve no purpose (the destructor handles it).

### M-3. No validation that plugin directory is not the build directory itself

**File:** `src/cli.cpp:156-159`

The CLI validation checks that `--plugin-dir` exists and is a directory, but does not guard against accidentally pointing to a directory containing non-plugin shared libraries (e.g., the build directory itself). While `loadPlugins()` gracefully skips files without required symbols, it will attempt to `dlopen()` every `.so`/`.dylib` in the directory, which could have side effects (constructors in shared libraries run on load).

**Suggested fix:** Consider logging a warning if the plugin directory contains files that are not loadable plugins, and/or supporting a naming convention (e.g., `llama_plugin_*.so`) to reduce accidental loading.

### M-4. `ReadSeek::read()` parameter order mismatch between C++ and C API is a trap

**File:** `include/plugin_api.h:19`, `include/readseek.h:16-17`

The C++ `ReadSeek::read` signature is `read(size_t len, uint8_t* buf)` (length first), while the C ABI `LlamaReadSeek.read` is `read(void* opaque, uint8_t* buf, size_t len)` (buffer first, then length). The `wrapReadSeek` bridge correctly handles this transposition. But any future maintainer writing a new bridge or plugin needs to be aware of this mismatch.

**Suggested fix:** Add a comment in `wrapReadSeek` noting the parameter order difference. Alternatively, change the C ABI to match the C++ order: `read(void* opaque, size_t len, uint8_t* buf)`.

### M-5. `seek()` return type inconsistency

**File:** `include/plugin_api.h:20`, `include/readseek.h:20`

The C ABI `seek` returns `int64_t` (signed, for error indication), while the C++ `ReadSeek::seek` returns `size_t` (unsigned, the new position). The bridge in `processor.cpp:54-60` catches exceptions and returns -1 on error, 0 on success. But it discards the return value from `ReadSeek::seek()` (the new position). A plugin calling `seek()` that wants to know the resulting position gets 0 on success, not the actual position.

**Suggested fix:** Return the actual position on success:

```cpp
lrs.seek = [](void* o, size_t pos) -> int64_t {
  try {
    return static_cast<int64_t>(static_cast<ReadSeek*>(o)->seek(pos));
  } catch (...) {
    return -1;
  }
};
```

### M-6. Test stub plugin does not exercise error paths

**File:** `test/test_plugin_stub.c`

The test stub always returns 0 from `llama_plugin_process`. There are no tests for the error path where a plugin returns -1 and `lastError()` is consulted. The `testPluginManagerProcessFile` test sends a "SQLite Database" signature that triggers a read, but the read succeeds because `ReadSeekBuf` has data.

**Suggested fix:** Add a second test plugin or a test case that triggers a plugin error return, verifying that `errorPlugin` and `errorMessage` are correctly populated.

### M-7. `test/meson.build` includes `posixreader.cpp` unconditionally

**File:** `test/meson.build:20`

The test build includes `../src/posixreader.cpp` unconditionally, while `src/meson.build:51-53` only includes it on Linux. This is a pre-existing issue (not introduced by the plugin commits) but may cause build failures on macOS/Windows if PosixReader has Linux-specific includes.

**Not a plugin-system issue**, but noted since the plugin commits touch `test/meson.build`.

---

## Observations

### O-1. Plan followed closely

The implementation follows the plan almost verbatim, including test names, file structure, function signatures, and commit messages. The main deviation is the addition of exception handling in `wrapReadSeek` lambdas (commit 53b0a5c), which was a sensible improvement not in the original plan. The plan's Task 9 (integration test with Processor) was implemented but simplified -- it does not create all the database tables the plan suggested, using `ruleEngine->createTables()` instead.

### O-2. No `dl` library dependency declared

**File:** `meson.build`

Boost.DLL internally uses `dlopen`/`dlsym` on POSIX systems, which requires linking against `libdl` on some Linux distributions. The `meson.build` does not explicitly declare a dependency on `dl`. On macOS, `dlopen` is part of libSystem and needs no extra linking. On some Linux systems, Boost headers may handle this automatically, or it may fail at link time. This should be verified on the target Linux build environment.

### O-3. Plugin loading order is filesystem-dependent

**File:** `src/pluginmanager.cpp:23`

`std::filesystem::directory_iterator` does not guarantee ordering. Plugins will be loaded (and therefore called during `processFile()`) in whatever order the filesystem returns them. Combined with finding I-3 (first error stops processing), this means the effective behavior depends on filesystem ordering.

### O-4. Boost.DLL diagnostic suppression is appropriate

**File:** `include/boost_dll.h`

The wrapper header suppresses `-Wnon-virtual-dtor` and `-Wdeprecated-declarations` for the Boost.DLL includes. This is a reasonable approach for managing upstream header warnings without disabling them project-wide.

### O-5. The `LlamaReadSeek.size` callback does not handle exceptions

**File:** `src/processor.cpp:62-64`

The `size` callback in `wrapReadSeek` does not have a try/catch, unlike `read` and `seek`. If `ReadSeek::size()` throws, the exception will propagate through the C ABI boundary (undefined behavior). In practice, `ReadSeek::size()` implementations in the codebase (`ReadSeekBuf`, `ReadSeekFile`, `ReadSeekTSK`) are unlikely to throw, but the inconsistency is worth noting.

**Suggested fix:** Add the same try/catch pattern, returning 0 on exception.

---

## What Went Well

1. **Clean C ABI design.** The `plugin_api.h` header is pure C, compiles as both C11 and C++20, uses standard types, and has clear ownership semantics. The `LLAMA_PLUGIN_EXPORT` macro handles platform differences.

2. **Defensive symbol checking.** `loadPlugins()` checks for all four required symbols before calling any of them, and gracefully skips libraries that are not valid plugins. This prevents crashes from loading unrelated shared libraries.

3. **Correct lifecycle ordering.** Plugins are loaded after DB init (`loadPlugins()` runs after all futures resolve), and shut down after thread pool join but before DB export. This ensures plugins have valid DB connections and all processing is complete before cleanup.

4. **Exception handling at the C/C++ boundary.** The `wrapReadSeek` bridge in `processor.cpp` catches C++ exceptions and converts them to error return codes before crossing into C plugin code (added in the final commit). This is exactly the right pattern for C ABI safety.

5. **Reverse-order shutdown.** `PluginManager::shutdown()` iterates plugins in reverse order, which is the correct LIFO cleanup pattern for dependencies.

6. **Good test coverage for the happy path.** The tests cover: no plugins, empty directory, loading the stub, processing matching and non-matching files, and the integration test with the full Processor pipeline. The `PLUGIN_STUB_DIR` compile definition is a clean way to locate the test plugin.

7. **Non-invasive integration.** The plugin dispatch is guarded by `if (Context->Plugins)`, meaning the existing pipeline is completely unaffected when no plugins are configured. Zero overhead for the no-plugin case.

8. **Build system changes are minimal and correct.** `pluginmanager.cpp` is added to both `src/meson.build` and `test/meson.build`, the test stub is built as a shared library with `name_prefix: ''` to avoid `lib` prefix issues, and the compile definition passes the build directory path correctly.

---

## Final Assessment

The plugin system is a solid first implementation suitable for merging to the development branch. The architecture is clean, the code follows the plan faithfully, and the test coverage is reasonable for an initial implementation.

**Before production use**, address:
- **C-2** (double-pointer issue) -- this will cause real bugs the moment a plugin tries to use the DB handle
- **I-2** (catch exceptions from plugin calls) -- defensive measure against misbehaving plugins
- **I-4** (API version field) -- needed before any third-party plugins are written

**Nice-to-have improvements:**
- **C-1** addendum: document the thread-safety contract, consider making `processFile()` const
- **I-3**: allow plugins to fail independently
- **M-1**: extract `wrapReadSeek` to a shared location
- **M-5**: return actual seek position
- **M-6**: add tests for plugin error paths

The code is well-organized, the commit history is clean and logical, and the implementation demonstrates good engineering judgment in areas like lifecycle ordering and C/C++ boundary safety.
