# Phase 3: Local Optimizations Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Implement local performance optimizations with A/B benchmarks to validate improvements: SQL string pre-allocation, hash memoization, and string comparison hashing.

**Architecture:** Each optimization is implemented with the old code preserved temporarily for A/B benchmarking. After validating improvement, old code is removed. All changes are local to single files or small areas.

**Tech Stack:** C++17, Catch2 benchmarking, Meson build system

---

## Optimization 1: SQL String Pre-allocation

### Task 1: Add A/B benchmark for SQL generation

**Files:**
- Modify: `test/benchmarks/test_parser.cpp`

**Step 1: Add A/B benchmark comparing current vs pre-allocated**

Add after existing SQL benchmark (or modify SqlGenerationBenchmark):
```cpp
TEST_CASE("SqlGenerationABBenchmark") {
  std::string corpus = generateRules(100);

  RuleReader r;
  REQUIRE(r.read(corpus, "benchmark"));

  QueryBuilder qb(r.getParser());
  const auto& rules = r.getRules();
  const std::string sqlUnion(" UNION ");

  // Baseline: current implementation (no pre-allocation)
  BENCHMARK("SQL-noPrealloc") {
    std::string hits_query("INSERT INTO rule_hits ");
    for (const Rule& rule : rules) {
      hits_query += "(";
      hits_query += qb.buildSqlQuery(rule);
      hits_query += ")";
      hits_query += sqlUnion;
    }
    hits_query.erase(hits_query.size() - sqlUnion.size());
    hits_query += ";";
    return hits_query.size();
  };

  // Optimized: with pre-allocation
  BENCHMARK("SQL-prealloc") {
    std::string hits_query;
    // ~200 chars per rule SQL + overhead
    hits_query.reserve(rules.size() * 220 + 50);
    hits_query = "INSERT INTO rule_hits ";
    for (const Rule& rule : rules) {
      hits_query += "(";
      hits_query += qb.buildSqlQuery(rule);
      hits_query += ")";
      hits_query += sqlUnion;
    }
    hits_query.erase(hits_query.size() - sqlUnion.size());
    hits_query += ";";
    return hits_query.size();
  };
}
```

**Step 2: Build and run A/B benchmark**

Run: `meson compile -C builddir parser_benchmarks && ./builddir/test/parser_benchmarks "[SqlGenerationABBenchmark]" --benchmark-samples 20`
Expected: prealloc version shows improvement

**Step 3: Commit**

```bash
git add test/benchmarks/test_parser.cpp
git commit -m "test: add A/B benchmark for SQL string pre-allocation"
```

---

### Task 2: Implement SQL string pre-allocation

**Files:**
- Modify: `src/ruleengine.cpp:16-17`

**Step 1: Add pre-allocation before the loop**

Change lines 15-17 from:
```cpp
  duckdb_result result;
  std::string hits_query("INSERT INTO rule_hits ");
  const std::string sqlUnion(" UNION ");
```

to:
```cpp
  duckdb_result result;
  std::string hits_query;
  // Pre-allocate: ~200 chars per rule SQL + INSERT prefix + semicolon
  hits_query.reserve(Reader.getRules().size() * 220 + 50);
  hits_query = "INSERT INTO rule_hits ";
  const std::string sqlUnion(" UNION ");
```

**Step 2: Build and run tests**

Run: `meson compile -C builddir && meson test -C builddir --verbose`
Expected: All tests pass

**Step 3: Run benchmark to confirm improvement**

Run: `./builddir/test/parser_benchmarks "[SqlGenerationABBenchmark]" --benchmark-samples 20`
Expected: Both versions now show similar performance (production code uses pre-allocation)

**Step 4: Commit**

```bash
git add src/ruleengine.cpp
git commit -m "perf: pre-allocate SQL query string to avoid reallocations

Reserves capacity based on rule count before building query string,
eliminating O(n²) reallocation behavior."
```

---

### Task 3: Remove A/B benchmark baseline (optional cleanup)

**Files:**
- Modify: `test/benchmarks/test_parser.cpp`

**Step 1: Simplify benchmark to single version**

The A/B benchmark can be simplified once the optimization is validated. Keep the pre-allocated version as the standard benchmark.

**Step 2: Commit**

```bash
git add test/benchmarks/test_parser.cpp
git commit -m "test: simplify SQL benchmark after validating pre-allocation"
```

---

## Optimization 2: Hash Memoization

### Task 4: Add A/B benchmark for hash calculation

**Files:**
- Modify: `test/benchmarks/test_parser.cpp`

**Step 1: Add A/B benchmark for hash memoization**

```cpp
TEST_CASE("HashMemoizationABBenchmark") {
  std::string corpus = generateRules(100);

  RuleReader r;
  REQUIRE(r.read(corpus, "benchmark"));

  const auto& rules = r.getRules();
  const auto& parser = r.getParser();

  // Simulate calling getHash twice per rule (like writeRulesToDb + buildFsm)
  BENCHMARK("getHash-2xPerRule") {
    FieldHash h;
    for (const Rule& rule : rules) {
      h = rule.getHash(parser);  // First call
      h = rule.getHash(parser);  // Second call (simulating buildFsm)
    }
    return h.to_string().size();
  };
}
```

**Step 2: Build and run benchmark**

Run: `meson compile -C builddir parser_benchmarks && ./builddir/test/parser_benchmarks "[HashMemoizationABBenchmark]" --benchmark-samples 20`
Expected: Establishes baseline for double-call pattern

**Step 3: Commit**

```bash
git add test/benchmarks/test_parser.cpp
git commit -m "test: add benchmark for hash memoization A/B testing"
```

---

### Task 5: Add CachedHash field to Rule struct

**Files:**
- Modify: `include/parser.h:159-175`

**Step 1: Add optional header include**

After the existing includes (around line 11), add:
```cpp
#include <optional>
```

**Step 2: Add CachedHash field to Rule struct**

Change the Rule struct from:
```cpp
struct Rule {
  // Used for unique rule ID in the database.
  FieldHash getHash(const LlamaParser&) const;
  const std::unordered_map<std::string_view, PatternDef>& getPatternMap() const { return Grep.Patterns.Patterns; }

  std::string_view      Name;
  MetaSection           Meta;
  HashSection           Hash;
  std::shared_ptr<Node> Signature;
  std::shared_ptr<Node> FileMetadata;
  GrepSection           Grep;

  // Relative input offset where the Meta section ends and the first "real" section begins.
  uint64_t Start = 0;
  // Relative input offset right before the rule's closing brace.
  uint64_t End = 0;
};
```

to:
```cpp
struct Rule {
  // Used for unique rule ID in the database.
  FieldHash getHash(const LlamaParser&) const;
  const std::unordered_map<std::string_view, PatternDef>& getPatternMap() const { return Grep.Patterns.Patterns; }

  std::string_view      Name;
  MetaSection           Meta;
  HashSection           Hash;
  std::shared_ptr<Node> Signature;
  std::shared_ptr<Node> FileMetadata;
  GrepSection           Grep;

  // Relative input offset where the Meta section ends and the first "real" section begins.
  uint64_t Start = 0;
  // Relative input offset right before the rule's closing brace.
  uint64_t End = 0;

  // Cached hash value for memoization
  mutable std::optional<FieldHash> CachedHash;
};
```

**Step 3: Build to verify header change**

Run: `meson compile -C builddir`
Expected: Build succeeds

**Step 4: Commit**

```bash
git add include/parser.h
git commit -m "feat: add CachedHash field to Rule for memoization"
```

---

### Task 6: Implement hash memoization in getHash

**Files:**
- Modify: `src/parser.cpp:71-75`

**Step 1: Add memoization logic**

Change from:
```cpp
FieldHash Rule::getHash(const LlamaParser& parser) const {
  FieldHasher hasher;
  hasher.hash_iter(parser.Tokens.begin() + Start, parser.Tokens.begin() + End, [](const Token& token) { return token.Lexeme; });
  return hasher.get_hash();
}
```

to:
```cpp
FieldHash Rule::getHash(const LlamaParser& parser) const {
  if (!CachedHash) {
    FieldHasher hasher;
    hasher.hash_iter(parser.Tokens.begin() + Start, parser.Tokens.begin() + End, [](const Token& token) { return token.Lexeme; });
    CachedHash = hasher.get_hash();
  }
  return *CachedHash;
}
```

**Step 2: Build and run tests**

Run: `meson compile -C builddir && meson test -C builddir --verbose`
Expected: All tests pass

**Step 3: Run benchmark to confirm improvement**

Run: `./builddir/test/parser_benchmarks "[HashMemoizationABBenchmark]" --benchmark-samples 20`
Expected: Significant improvement (second call is now O(1))

**Step 4: Commit**

```bash
git add src/parser.cpp
git commit -m "perf: memoize Rule::getHash to avoid redundant calculations

Hash is computed on first call and cached. Subsequent calls return
cached value. Reduces hash calculations by 50%+ in typical usage."
```

---

## Optimization 3: String Comparison Hashing

### Task 7: Add FNV-1a hash function to parser.h

**Files:**
- Modify: `include/parser.h`

**Step 1: Add constexpr FNV-1a hash function**

Add before the ParserError class (around line 17):
```cpp
// Compile-time FNV-1a hash for keyword comparison optimization
constexpr uint64_t fnv1a(std::string_view str) {
  uint64_t hash = 14695981039346656037ULL;
  for (char c : str) {
    hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
    hash *= 1099511628211ULL;
  }
  return hash;
}
```

**Step 2: Build to verify**

Run: `meson compile -C builddir`
Expected: Build succeeds

**Step 3: Commit**

```bash
git add include/parser.h
git commit -m "feat: add constexpr FNV-1a hash function for keyword comparison"
```

---

### Task 8: Add A/B benchmark for checkFunctionName

**Files:**
- Modify: `test/benchmarks/test_parser.cpp`

**Step 1: Add A/B benchmark**

```cpp
TEST_CASE("CheckFunctionNameABBenchmark") {
  // Create a string with many identifiers to check
  std::string input;
  for (int i = 0; i < 100; ++i) {
    input += "any all offset count count_has_hits length foo bar baz ";
  }

  LlamaLexer lexer;
  lexer.setInput(input);
  lexer.scanTokens();

  LlamaParser parser(input, lexer.tokens());

  // String comparison version (baseline)
  auto checkFunctionNameBaseline = [&]() {
    std::string_view curLex = parser.currentLexeme();
    return (
      curLex == "any"            ||
      curLex == "all"            ||
      curLex == "offset"         ||
      curLex == "count"          ||
      curLex == "count_has_hits" ||
      curLex == "length"
    );
  };

  // Hash comparison version
  constexpr uint64_t H_ANY = fnv1a("any");
  constexpr uint64_t H_ALL = fnv1a("all");
  constexpr uint64_t H_OFFSET = fnv1a("offset");
  constexpr uint64_t H_COUNT = fnv1a("count");
  constexpr uint64_t H_COUNT_HAS_HITS = fnv1a("count_has_hits");
  constexpr uint64_t H_LENGTH = fnv1a("length");

  auto checkFunctionNameHashed = [&]() {
    uint64_t h = fnv1a(parser.currentLexeme());
    return (
      h == H_ANY            ||
      h == H_ALL            ||
      h == H_OFFSET         ||
      h == H_COUNT          ||
      h == H_COUNT_HAS_HITS ||
      h == H_LENGTH
    );
  };

  int matchCount = 0;

  BENCHMARK("checkFunctionName-strings") {
    matchCount = 0;
    for (size_t i = 0; i < parser.Tokens.size(); ++i) {
      parser.CurIdx = i;
      if (checkFunctionNameBaseline()) {
        ++matchCount;
      }
    }
    return matchCount;
  };

  BENCHMARK("checkFunctionName-hashed") {
    matchCount = 0;
    for (size_t i = 0; i < parser.Tokens.size(); ++i) {
      parser.CurIdx = i;
      if (checkFunctionNameHashed()) {
        ++matchCount;
      }
    }
    return matchCount;
  };

  CHECK(matchCount > 0);
}
```

**Step 2: Build and run benchmark**

Run: `meson compile -C builddir parser_benchmarks && ./builddir/test/parser_benchmarks "[CheckFunctionNameABBenchmark]" --benchmark-samples 20`
Expected: Hashed version shows improvement

**Step 3: Commit**

```bash
git add test/benchmarks/test_parser.cpp
git commit -m "test: add A/B benchmark for checkFunctionName hashing"
```

---

### Task 9: Implement hashed checkFunctionName

**Files:**
- Modify: `include/parser.h:311-321`

**Step 1: Replace checkFunctionName with hashed version**

Change from:
```cpp
  bool checkFunctionName() {
    std::string_view curLex = currentLexeme();
    return (
      curLex == "any"            ||
      curLex == "all"            ||
      curLex == "offset"         ||
      curLex == "count"          ||
      curLex == "count_has_hits" ||
      curLex == "length"
    );
  }
```

to:
```cpp
  bool checkFunctionName() {
    static constexpr uint64_t H_ANY = fnv1a("any");
    static constexpr uint64_t H_ALL = fnv1a("all");
    static constexpr uint64_t H_OFFSET = fnv1a("offset");
    static constexpr uint64_t H_COUNT = fnv1a("count");
    static constexpr uint64_t H_COUNT_HAS_HITS = fnv1a("count_has_hits");
    static constexpr uint64_t H_LENGTH = fnv1a("length");

    uint64_t h = fnv1a(currentLexeme());
    return (
      h == H_ANY            ||
      h == H_ALL            ||
      h == H_OFFSET         ||
      h == H_COUNT          ||
      h == H_COUNT_HAS_HITS ||
      h == H_LENGTH
    );
  }
```

**Step 2: Build and run tests**

Run: `meson compile -C builddir && meson test -C builddir --verbose`
Expected: All tests pass

**Step 3: Commit**

```bash
git add include/parser.h
git commit -m "perf: use FNV-1a hashing in checkFunctionName

Replaces 6 string comparisons with 1 hash computation and 6 integer
comparisons. Hash values are computed at compile time."
```

---

### Task 10: Implement hashed checkSignatureProperty

**Files:**
- Modify: `include/parser.h:323-326`

**Step 1: Replace with hashed version**

Change from:
```cpp
  bool checkSignatureProperty() {
    std::string_view curLex = currentLexeme();
    return (curLex == "name" || curLex == "id");
  }
```

to:
```cpp
  bool checkSignatureProperty() {
    static constexpr uint64_t H_NAME = fnv1a("name");
    static constexpr uint64_t H_ID = fnv1a("id");

    uint64_t h = fnv1a(currentLexeme());
    return (h == H_NAME || h == H_ID);
  }
```

**Step 2: Build and run tests**

Run: `meson compile -C builddir && meson test -C builddir --verbose`
Expected: All tests pass

**Step 3: Commit**

```bash
git add include/parser.h
git commit -m "perf: use FNV-1a hashing in checkSignatureProperty"
```

---

### Task 11: Implement hashed checkFileMetadataProperty

**Files:**
- Modify: `include/parser.h:328-337`

**Step 1: Replace with hashed version**

Change from:
```cpp
  bool checkFileMetadataProperty() {
    std::string_view curLex = currentLexeme();
    return (
      curLex == "created"  ||
      curLex == "modified" ||
      curLex == "filesize" ||
      curLex == "filepath" ||
      curLex == "filename"
    );
  }
```

to:
```cpp
  bool checkFileMetadataProperty() {
    static constexpr uint64_t H_CREATED = fnv1a("created");
    static constexpr uint64_t H_MODIFIED = fnv1a("modified");
    static constexpr uint64_t H_FILESIZE = fnv1a("filesize");
    static constexpr uint64_t H_FILEPATH = fnv1a("filepath");
    static constexpr uint64_t H_FILENAME = fnv1a("filename");

    uint64_t h = fnv1a(currentLexeme());
    return (
      h == H_CREATED  ||
      h == H_MODIFIED ||
      h == H_FILESIZE ||
      h == H_FILEPATH ||
      h == H_FILENAME
    );
  }
```

**Step 2: Build and run tests**

Run: `meson compile -C builddir && meson test -C builddir --verbose`
Expected: All tests pass

**Step 3: Commit**

```bash
git add include/parser.h
git commit -m "perf: use FNV-1a hashing in checkFileMetadataProperty"
```

---

### Task 12: Final verification and benchmark comparison

**Step 1: Run full test suite**

Run: `meson test -C builddir --verbose`
Expected: All tests pass

**Step 2: Run all benchmarks**

Run: `./builddir/test/parser_benchmarks --benchmark-samples 20`
Expected: All benchmarks run, optimized versions show improvement

**Step 3: Capture post-optimization results**

Run: `./builddir/test/parser_benchmarks --benchmark-samples 20 --reporter xml > docs/plans/phase3-benchmark-results.xml`

**Step 4: Commit results**

```bash
git add docs/plans/phase3-benchmark-results.xml
git commit -m "docs: capture Phase 3 optimization benchmark results"
```

**Step 5: Review commit history**

Run: `git log --oneline -12`
Expected: See all Phase 3 commits
