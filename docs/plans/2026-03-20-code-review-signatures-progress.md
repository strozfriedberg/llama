# Code Review: File Signature Analysis & Progress Bar

**Date:** 2026-03-20
**Scope:** All changes from commit `3584671` (file signature design doc) through `20ce351` (progress bar tuning) — ~30 commits, ~43 files, ~2350 lines added.
**Branch:** `progress-bar` (not yet merged to `main`)

---

## Overview

This review covers two features and their supporting changes:

### File Signature Analysis
The signature system reads magic byte definitions from a JSON file (`magics.json`), compiles them into a Lightgrep finite state machine, and runs the FSM against the first 4096 bytes of every file during processing. Matches are stored in DuckDB (`file_signatures` table) and can be referenced in Llama rules via `signature` sections. The compiled FSM program can be cached to disk for reuse.

**Key files:**
- `include/filesignatures.h` / `src/filesignatures.cpp` — `Magic` struct, `Lightgrep` wrapper class, `FileSigAnalyzer`
- `include/ducksig.h` — `SigRec` and `FileSigResult` DB record structs
- `src/processor.cpp` — Per-file signature detection in `Processor::process()`
- `src/querybuilder.cpp` — SQL generation for `signature` rule sections
- `src/ruleengine.cpp` — Table creation for `signatures` and `file_signatures`
- `src/llama.cpp` — CLI option, init/loading, signature metadata insertion

**Data flow:**
```
magics.json → readMagics() → MagicsType (vector<shared_ptr<Magic>>)
                                ↓
                         Lightgrep::compile() → shared_ptr<ProgramHandle>
                                ↓
              ProcessorContext stores both SigMagics + SigProg
                                ↓
              Each Processor creates its own FileSigAnalyzer from them
                                ↓
    Processor::process() → SigAnalyzer.getSignatures(stream, results)
                                ↓
              results → FileSigResult records → DuckDB file_signatures table
                                ↓
    QueryBuilder::buildSqlQuery() generates SQL subquery joining
    file_signatures ↔ signatures tables for rule evaluation
```

### Progress Bar
A polling-based progress display that shows processing rates on stderr when running interactively. `ProgressInfo` holds atomic counters updated by worker threads (via `Processor::processBatch()`). `ProgressThread` polls these counters every 200ms and formats a status line.

**Key files:**
- `include/progressinfo.h` / `src/progressinfo.cpp` — Atomic counters + formatting
- `include/progressthread.h` / `src/progressthread.cpp` — Polling thread
- `src/llama.cpp` — Wiring in `search()`
- `src/tskreader.cpp` — Per-filesystem progress metadata

**Threading model:**
```
Main thread:    creates ProgressInfo (stack local)
                starts ProgressThread (polls every 200ms, writes to stderr)
                starts InputReader (calls setFilesystem() per FS)
Worker threads: Processor::processBatch() calls ProgressInfo::update()
                (atomic fetch_add, batched every 128KB)
Main thread:    Pool.join() → progressThread.stop() → final stats line
```

### Supporting Changes
- `DirentStack::push()` now returns `std::optional<Dirent>` to handle `.` and `..` entries without polluting paths
- `InputReader` base class gained `setProgressInfo()` virtual method
- `TskReader` reports per-filesystem metadata to ProgressInfo
- Various bug fixes (flush clearing, TSK inode conversion, SEGV fix)

---

## 1. Bugs & Correctness Issues

### 1a. CRITICAL: `duckdb_result` leak in `ruleengine.cpp:40`

`writeRulesToDb()` calls `duckdb_query()` into a `duckdb_result result` but never calls `duckdb_destroy_result(&result)`. Every other callsite in the codebase destroys the result. This leaks memory proportional to the query result size.

**File:** `src/ruleengine.cpp:40-41`

### 1c. `buildSqlQuery` references `hash.Blake3` — table not in FROM clause

```cpp
query += " AND hash.Blake3 IN (SELECT fs.FileHash FROM file_signatures fs"
```

The FROM clause is `FROM dirent, inode WHERE dirent.Metaaddr == inode.Addr`. There is no `hash` table in the FROM. This query will fail at runtime for any rule with a `signature` section. The `hash` table needs to be joined, or the column reference needs to use a subquery that reaches the hash table.

**File:** `src/querybuilder.cpp:129`

### 1d. Unchecked `.find()` in QueryBuilder — UB on unknown property names

```cpp
out += FileMetadataPropertySqlLookup.find(propertyName)->second;  // line 41
out += SignaturePropertySqlLookup.find(propertyName)->second;      // line 80
```

If the property name isn't in the map, `.find()` returns `end()`, and dereferencing `end()->second` is undefined behavior. The parser validates property names, so this *shouldn't* happen in practice, but defense in depth says we should check the iterator and throw a descriptive error rather than crash.

**File:** `src/querybuilder.cpp:41, 80`

### 1e. `ProgressThread::StartTime` race condition

`StartTime` is a plain `Clock::time_point` member written in `run()` (thread, line 32) and read in `stop()` (main thread, line 25) with no synchronization. If `stop()` executes before `run()` has set `StartTime`, the elapsed time calculation uses an uninitialized value. Even if timing makes this unlikely, it's formally a data race (UB per C++ memory model).

**Fix:** Initialize `StartTime` in `start()` before spawning the thread. This is safe because `start()` runs on the main thread before `run()` begins, and thread creation is a synchronization point.

**File:** `src/progressthread.cpp:25, 32`

### 1f. `startsWith()` orphan utility

`startsWith()` in `filesignatures.h:71-73` is unused in production code (only referenced in test data files). C++20 provides `std::string::starts_with()`. Should be removed.

**File:** `include/filesignatures.h:71-73`

### 1g. Unused `#include <map>` in `filesignatures.h:4`

`<map>` is included but only `<unordered_map>` is used (in the `Magic::Extensions` member).

**File:** `include/filesignatures.h:4`

### 1h. `ReadBuf` marked `mutable` unnecessarily

`FileSigAnalyzer::ReadBuf` is `mutable` but `getSignatures()` is not `const`. The `mutable` keyword is misleading — it suggests the buffer is modified in const contexts, which isn't the case.

**File:** `include/filesignatures.h:56`

---

## 2. Refactoring Opportunities

### 2a. HIGH: QueryBuilder signature/file_metadata clause duplication

`buildSqlClauseImpl(PropertyNode*)` (lines 39-54) and `buildSignaturePropertyImpl(PropertyNode*)` (lines 78-93) are **identical** except for which lookup map they use. Similarly, `buildSqlClauseImpl(Node*)` (lines 16-30) and `buildSignatureClauseImpl(Node*)` (lines 95-114) are identical in structure — same switch on NodeType, same recursive descent, same string building.

**Refactor:** Extract a common implementation that takes the lookup table as a parameter:

```cpp
using PropertyLookup = std::unordered_map<std::string_view, std::string>;

void QueryBuilder::buildClauseImpl(const Node* n, const PropertyLookup& lookup, std::string& out);
void QueryBuilder::buildPropertyImpl(const PropertyNode* pn, const PropertyLookup& lookup, std::string& out);
```

Then `buildSqlClauseImpl` and `buildSignatureClauseImpl` become one-liners that pass the appropriate lookup table. This eliminates ~30 lines of duplicated logic and prevents the two paths from silently diverging.

**File:** `src/querybuilder.cpp:16-114`

### 2b. MEDIUM: `ProcessorContext` constructor has too many parameters (8)

```cpp
ProcessorContext(LlamaDB* db, const shared_ptr<ProgramHandle>& prog,
  const shared_ptr<LlamaRuleEngine> ruleEngine,
  const string& exclusionHsetPath, const string& inclusionHsetPath,
  const MagicsType& sigMagics, const shared_ptr<ProgramHandle>& sigProg,
  ProgressInfo* progress);
```

The callsite in `llama.cpp:91` is already a very long line and will grow as features are added. Consider grouping related params into small structs, or just passing `Options` directly since most of these come from there.

**Files:** `include/processor.h:27-35`, `src/llama.cpp:91`

### 2c. MEDIUM: `Processor` members that should be `unique_ptr` instead of `shared_ptr`

The code itself documents this with comments:
```cpp
std::shared_ptr<ContextHandle> LgCtx; // not shared, could be unique_ptr
std::shared_ptr<SFHASH_Hasher> Hasher; // not shared, could be unique_ptr
```

These should be `unique_ptr` with custom deleters. `shared_ptr` with custom deleter when there's no sharing is misleading about ownership semantics and has slightly higher overhead (control block allocation).

**File:** `include/processor.h:88-89`

### 2d. LOW: Unused `RuleMatchAppender` member

`RuleMatchAppender` is constructed in `Processor::Processor()` (opening a DuckDB appender) but never used. Rule matches are written through `RuleEngine::writeRulesToDb()` instead. This is a dead member that opens an unnecessary database resource.

**File:** `include/processor.h:85`, `src/processor.cpp:81`

### 2e. LOW: `formatSize` and `formatRate` magic number duplication

`1024.0 * 1024.0 * 1024.0` appears 4 times, `1024.0 * 1024.0` appears 3 times in `progressinfo.cpp`. Named constants would improve readability. Also, `formatRate()` only handles MB/s and GB/s — for slow operations (e.g., reading from a network share), it'll show "0.0 MB/s" rather than a more useful KB/s value.

**File:** `src/progressinfo.cpp:70-102`

---

## 3. Design Observations (not blocking, worth discussing)

### 3a. `setFilesystem()` has surprising side effects

`ProgressInfo::setFilesystem()` stores filesystem metadata AND resets `InodesProcessed` and `BytesProcessed` to zero. The name doesn't communicate the reset. Two options: (1) rename to `startFilesystem()`, or (2) separate `resetCounters()` from `setFilesystem()`.

### 3b. `setProgressInfo()` duplicated across readers

`DirReader`, `TskReader`, and `PosixReader` all have identical one-liner implementations of `setProgressInfo()`. The base class `InputReader` already has a default (empty) implementation. If the default stored the pointer, all three overrides could be removed.

### 3c. Progress bar tests don't exercise the threading path

Both tests in `test_progressthread.cpp` pass `isTty=false`, so the thread never starts. They verify "no crash when disabled" but not the actual behavior. A test with `isTty=true` that verifies start/stop lifecycle would catch regressions like the StartTime race (1e).

### 3d. Memory ordering could be relaxed

All atomics in `ProgressInfo` use `seq_cst` (the default). For independent counters updated via `fetch_add` and read via `load` for display purposes, `memory_order_relaxed` would be sufficient. The difference is negligible on x86 (where seq_cst stores are cheap) but matters more on ARM. Not urgent, but worth a comment explaining the conscious choice.

### 3e. `Lightgrep` class naming

`Lightgrep` manages a compiled LG program handle and a search context. The name collides conceptually with the actual lightgrep library. Something like `PatternProgram` or `CompiledPatterns` would better describe what the class actually is — a compiled pattern set ready for searching.

---

## 4. Recommended Priority

| Priority | Item | Effort |
|----------|------|--------|
| **Fix now** | 1a. duckdb_result leak | 1 line |
| **Fix now** | 1c. hash.Blake3 missing FROM | Small (fix SQL join) |
| **Fix now** | 1d. Unchecked .find() UB | Small |
| **Fix now** | 1e. StartTime race | Small |
| **Fix now** | 1f. Remove orphan startsWith | 1 line |
| **Fix now** | 1g. Remove unused include | 1 line |
| **Fix now** | 1h. Remove mutable | 1 line |
| **Soon** | 1b. Hard-coded PDF UUID | Medium (use Tags) |
| **Soon** | 2a. QueryBuilder dedup | Medium |
| **Soon** | 2d. Remove dead RuleMatchAppender | Small |
| **Later** | 2b. ProcessorContext params | Medium |
| **Later** | 2c. shared_ptr → unique_ptr | Small |
| **Later** | 2e. Format helpers | Small |
| **Later** | 3a-3e design items | Varies |

---

## Verification

After fixes:
1. `make -j8 check` — all tests pass
2. Run llama on a test image with a rule containing a `signature` section — verify the SQL query executes correctly end-to-end
3. Run llama with `--signatures` on a real disk image — verify progress bar displays correctly and final stats persist
4. Check for duckdb memory leaks with ASAN or a long-running analysis
