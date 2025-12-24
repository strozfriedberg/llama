# Parser Improvements Design

## Overview

This document covers two related efforts: error handling cleanup (API improvements) and parser performance optimization. The error handling work should be completed first as it addresses API concerns; performance work follows with proper benchmarking infrastructure to validate improvements.

## Phase 1: Error Handling Cleanup

### 1.1 Remove `LastError` from RuleReader

The `LastError` field in `RuleReader` is vestigial - it's never populated, only cleared. Errors are already accessible via `getParser().errors()` and `getLexer().errors()`.

**Files:**
- `include/rulereader.h`: Remove `LastError` field (line 21), `getLastError()` (line 15), `LastError.clear()` from `clear()` (line 12)
- `test/test_rulereader.cpp`: Remove assertion checking `getLastError()` (line 29)
- `test/benchmarks/test_parser.cpp`: Remove assertion checking `getLastError()` (line 100)

### 1.2 Flatten ParserError Inheritance

Current hierarchy:
```
std::runtime_error
  └── UnexpectedInputError (has messageWithPos helper)
        └── ParserError
```

Target hierarchy:
```
std::runtime_error
  ├── UnexpectedInputError
  └── ParserError
```

Extract `messageWithPos()` to a free function that both error types can use. This makes the exception types independent and the hierarchy flatter.

**Files:**
- `include/token.h`: Extract `messageWithPos()` as free function
- `include/parser.h`: Make `ParserError` inherit from `std::runtime_error` directly, use free function

### 1.3 Add Error Reporting to readRulesFromDir()

Currently `readRulesFromDir()` returns bool and silently accumulates rules. Add proper error reporting with filename context.

**Files:**
- `include/ruleengine.h`: Add `const RuleReader& getReader() const { return Reader; }`
- `src/llama.cpp`: In `readRulesFromDir()`, after each `engine->read()` call, iterate over lexer and parser errors and print with filename: `{filename}: {error.what()}`

## Phase 2: Benchmarking Infrastructure

### 2.1 Expand Test Corpus

The current benchmark uses 4 rules (~175 tokens). Add a larger corpus (100+ rules) for realistic workload testing. Options:
- Generate synthetic rules programmatically
- Use rules from `test/data/yara/` translated to llama format
- Create a dedicated benchmark rule file

### 2.2 Component-Level Benchmarks

Add benchmarks for user-visible performance:
- `readRulesFromDir()` with multiple files (captures file I/O + parsing)
- SQL query generation (`writeRulesToDb` or isolated `QueryBuilder` benchmark)

**File:** `test/benchmarks/test_parser.cpp`

### 2.3 Operation-Level Microbenchmarks

Add targeted benchmarks for operations we plan to optimize:
- `checkFunctionName()` string comparison
- `Rule::getHash()` calculation
- String concatenation in SQL generation

These provide before/after data for each optimization.

**File:** `test/benchmarks/test_parser.cpp`

### 2.4 Running Benchmarks

#### Build
```bash
meson compile -C builddir parser_benchmarks
```

#### Run all benchmarks
```bash
./builddir/test/parser_benchmarks
```

#### Run specific benchmark
```bash
./builddir/test/parser_benchmarks "LargeCorpusBenchmark"
```

#### Run with more samples for accuracy
```bash
./builddir/test/parser_benchmarks --benchmark-samples 100
```

#### Export to XML for tracking
```bash
./builddir/test/parser_benchmarks --reporter xml > benchmark-results.xml
```

### 2.5 A/B Comparison Pattern

For each optimization:
1. Keep both old and new implementations temporarily callable
2. Benchmark compares them in same test run
3. Once validated, remove old implementation

## Phase 3: Local Optimizations

### 3.1 SQL Query String Pre-allocation

**Current:** `ruleengine.cpp:16-27` builds query with repeated `+=` in a loop - accidentally quadratic.

**Change:** Pre-allocate string capacity before the loop:
```cpp
hits_query.reserve(Reader.getRules().size() * 220 + 50);
```

Estimate ~200 chars per rule SQL, add buffer for INSERT prefix and final semicolon.

**File:** `src/ruleengine.cpp`

### 3.2 Hash Memoization

**Current:** `Rule::getHash()` called twice per rule - in `writeRulesToDb()` (line 20) and `buildFsm()` (line 50).

**Change:** Add memoization to `Rule`:
```cpp
struct Rule {
  mutable std::optional<FieldHash> CachedHash;

  FieldHash getHash(const LlamaParser& parser) const {
    if (!CachedHash) {
      CachedHash = computeHash(parser);
    }
    return *CachedHash;
  }
  // ...
};
```

**File:** `include/parser.h` (Rule struct)

### 3.3 String Comparison Hashing

**Current:** `checkFunctionName()`, `checkSignatureProperty()`, `checkFileMetadataProperty()` use chains of `==` comparisons.

**Change:** Use compile-time FNV-1a hashing:
```cpp
constexpr uint64_t fnv1a(std::string_view str) {
  uint64_t hash = 14695981039346656037ULL;
  for (char c : str) {
    hash ^= static_cast<uint64_t>(c);
    hash *= 1099511628211ULL;
  }
  return hash;
}

bool checkFunctionName() {
  constexpr uint64_t ANY = fnv1a("any");
  constexpr uint64_t ALL = fnv1a("all");
  // ... other keywords

  uint64_t h = fnv1a(currentLexeme());
  return h == ANY || h == ALL || /* ... */;
}
```

**File:** `include/parser.h`

## Phase 4: Global Optimizations

### 4.1 Arena Allocation for AST Nodes

**Current:** AST nodes use `std::shared_ptr<Node>` with individual heap allocations.

**Change:** Store nodes in vectors as arenas, use indices instead of pointers:
```cpp
class LlamaParser {
  std::vector<BoolNode> BoolNodes;
  std::vector<FuncNode> FuncNodes;
  std::vector<PropertyNode> PropNodes;

  // Returns index into appropriate vector
  size_t allocBoolNode();
  size_t allocFuncNode();
  size_t allocPropNode();
  // ...
};

struct Node {
  NodeType Type;
  size_t Left = SIZE_MAX;   // Index, SIZE_MAX = null
  size_t Right = SIZE_MAX;
};
```

**Benefits:**
- Eliminates per-node heap allocation
- Better cache locality
- No reference counting overhead
- Simple cleanup (clear vectors)

**Files:**
- `include/parser.h`: Modify Node types to use indices, add arena vectors to LlamaParser
- `src/parser.cpp`: Update all node allocation to use arenas

### 4.2 Parallel File Loading

**Current:** `readRulesFromDir()` iterates files sequentially.

**Change:** Use `boost::asio::thread_pool` to parse files in parallel:
```cpp
bool readRulesFromDir(std::shared_ptr<LlamaRuleEngine> engine, const std::string& path) {
  boost::asio::thread_pool pool(std::thread::hardware_concurrency());
  std::vector<std::future<std::pair<bool, RuleReader>>> futures;

  for (const auto& file : std::filesystem::directory_iterator{path}) {
    futures.push_back(boost::asio::post(pool, std::packaged_task<...>([&file]() {
      RuleReader reader;
      std::string content = readfile(file.path().string());
      bool ok = reader.read(content, file.path().string());
      return {ok, std::move(reader)};
    })));
  }

  pool.join();

  // Merge results
  for (auto& fut : futures) {
    auto [ok, reader] = fut.get();
    // Collect rules and errors
  }
}
```

**Prerequisites:**
- Verify `RuleReader`, `LlamaLexer`, `LlamaParser` are reentrant (no global state)
- Synchronize error collection when merging

**File:** `src/llama.cpp`

## Not In Scope

- **Token iterator optimization**: Speculative benefit, invasive change. Deferred indefinitely pending profiling data showing token access as a bottleneck.
- **Custom allocators**: Replaced by simpler arena approach.

## Implementation Order Summary

1. Remove `LastError` from RuleReader
2. Flatten `ParserError` inheritance
3. Add error reporting to `readRulesFromDir()`
4. Add larger rule corpus for benchmarks
5. Add component-level benchmarks
6. Add operation-level microbenchmarks
7. SQL string pre-allocation (with A/B benchmark)
8. Hash memoization (with A/B benchmark)
9. String comparison hashing (with A/B benchmark)
10. Arena allocation for AST nodes (with A/B benchmark)
11. Parallel file loading (with A/B benchmark)
12. Profile for remaining hot spots
