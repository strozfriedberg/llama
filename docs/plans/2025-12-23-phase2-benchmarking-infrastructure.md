# Phase 2: Benchmarking Infrastructure Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Establish systematic benchmarks to measure baseline performance and validate future optimizations, covering component-level user-visible performance and operation-level hot paths.

**Architecture:** Extend existing Catch2 benchmarks in `test/benchmarks/test_parser.cpp`. Add a rule generator for larger test corpora. Create microbenchmarks for specific operations we plan to optimize (string comparison, hash calculation, SQL generation).

**Tech Stack:** C++17, Catch2 benchmarking, Meson build system

---

### Task 1: Create a rule generator for benchmark corpus

**Files:**
- Create: `test/benchmarks/rule_generator.h`

**Step 1: Write the rule generator header**

```cpp
#pragma once

#include <string>
#include <sstream>
#include <cstddef>

// Generates synthetic llama rules for benchmarking
inline std::string generateRules(size_t count) {
  std::ostringstream out;

  for (size_t i = 0; i < count; ++i) {
    out << "rule BenchmarkRule" << i << " {\n";
    out << "  meta:\n";
    out << "    description = \"Benchmark rule " << i << "\"\n";
    out << "    author = \"benchmark\"\n";
    out << "  file_metadata:\n";
    out << "    filesize > " << (i * 100 + 1000) << "\n";

    // Add grep section to every 3rd rule
    if (i % 3 == 0) {
      out << "  grep:\n";
      out << "    patterns:\n";
      out << "      p1 = \"pattern" << i << "\" fixed\n";
      out << "    condition:\n";
      out << "      any(p1)\n";
    }

    // Add hash section to every 5th rule
    if (i % 5 == 0) {
      out << "  hash:\n";
      out << "    sha256 == \"";
      // Generate a fake but valid-looking sha256 (64 hex chars)
      for (int j = 0; j < 64; ++j) {
        out << "0123456789abcdef"[(i + j) % 16];
      }
      out << "\"\n";
    }

    out << "}\n\n";
  }

  return out.str();
}
```

**Step 2: Build to verify header compiles**

Run: `meson compile -C builddir`
Expected: Build succeeds (header not yet included anywhere)

**Step 3: Commit**

```bash
git add test/benchmarks/rule_generator.h
git commit -m "feat: add rule generator for benchmark corpus"
```

---

### Task 2: Add large corpus benchmark for RuleReader

**Files:**
- Modify: `test/benchmarks/test_parser.cpp`

**Step 1: Include the rule generator**

Add after line 4:
```cpp
#include "rule_generator.h"
```

**Step 2: Add large corpus benchmark test**

Add after the existing `LlamaParserBenchmark` test (after line 101):
```cpp
TEST_CASE("LargeCorpusBenchmark") {
  // Generate 100 rules for realistic workload
  std::string largeCorpus = generateRules(100);

  RuleReader r;
  bool res = false;
  size_t ruleCount = 0;

  BENCHMARK("RuleReader-100rules") {
    res = r.read(largeCorpus, "benchmark");
    ruleCount = r.getRules().size();
    r.clear();
  };

  CHECK(res);
  CHECK(ruleCount == 100);
}

TEST_CASE("ScalingBenchmark") {
  // Test scaling behavior with different rule counts
  std::string corpus10 = generateRules(10);
  std::string corpus50 = generateRules(50);
  std::string corpus100 = generateRules(100);
  std::string corpus500 = generateRules(500);

  RuleReader r;

  BENCHMARK("RuleReader-10rules") {
    r.read(corpus10, "benchmark");
    r.clear();
  };

  BENCHMARK("RuleReader-50rules") {
    r.read(corpus50, "benchmark");
    r.clear();
  };

  BENCHMARK("RuleReader-100rules-scaling") {
    r.read(corpus100, "benchmark");
    r.clear();
  };

  BENCHMARK("RuleReader-500rules") {
    r.read(corpus500, "benchmark");
    r.clear();
  };
}
```

**Step 3: Build and run benchmarks**

Run: `meson compile -C builddir benchmarks && ./builddir/test/benchmarks "[LargeCorpusBenchmark]" --benchmark-samples 10`
Expected: Benchmarks run successfully

**Step 4: Commit**

```bash
git add test/benchmarks/test_parser.cpp
git commit -m "feat: add large corpus and scaling benchmarks"
```

---

### Task 3: Add SQL query generation benchmark

**Files:**
- Modify: `test/benchmarks/test_parser.cpp`

**Step 1: Add include for QueryBuilder**

Add after the rule_generator include:
```cpp
#include "querybuilder.h"
```

**Step 2: Add SQL generation benchmark**

Add after the ScalingBenchmark test:
```cpp
TEST_CASE("SqlGenerationBenchmark") {
  // Use rules with file_metadata to exercise SQL generation
  std::string corpus = generateRules(100);

  RuleReader r;
  REQUIRE(r.read(corpus, "benchmark"));

  QueryBuilder qb(r.getParser());
  const auto& rules = r.getRules();
  std::string query;

  BENCHMARK("buildSqlQuery-100rules") {
    std::string hits_query("INSERT INTO rule_hits ");
    const std::string sqlUnion(" UNION ");

    for (const Rule& rule : rules) {
      hits_query += "(";
      hits_query += qb.buildSqlQuery(rule);
      hits_query += ")";
      hits_query += sqlUnion;
    }
    hits_query.erase(hits_query.size() - sqlUnion.size());
    hits_query += ";";
    query = std::move(hits_query);
  };

  CHECK(!query.empty());
}
```

**Step 3: Build and run benchmark**

Run: `meson compile -C builddir benchmarks && ./builddir/test/benchmarks "[SqlGenerationBenchmark]" --benchmark-samples 10`
Expected: Benchmark runs successfully

**Step 4: Commit**

```bash
git add test/benchmarks/test_parser.cpp
git commit -m "feat: add SQL query generation benchmark"
```

---

### Task 4: Add checkFunctionName microbenchmark

**Files:**
- Modify: `test/benchmarks/test_parser.cpp`

**Step 1: Add microbenchmark for string comparison**

Add after SqlGenerationBenchmark:
```cpp
TEST_CASE("CheckFunctionNameBenchmark") {
  // Test the string comparison hot path
  std::string input = R"(
    rule TestRule {
      grep:
        patterns:
          p1 = "test"
        condition:
          any(p1) and all(p1) and count(p1) > 5
    }
  )";

  LlamaLexer lexer;
  lexer.setInput(input);
  lexer.scanTokens();

  LlamaParser parser(input, lexer.tokens());
  bool result = false;

  // Position parser at various function names and check
  BENCHMARK("checkFunctionName") {
    result = false;
    for (size_t i = 0; i < parser.Tokens.size(); ++i) {
      parser.CurIdx = i;
      if (parser.checkFunctionName()) {
        result = true;
      }
    }
  };

  CHECK(result); // Should have found at least one function name
}
```

**Step 2: Build and run benchmark**

Run: `meson compile -C builddir benchmarks && ./builddir/test/benchmarks "[CheckFunctionNameBenchmark]" --benchmark-samples 10`
Expected: Benchmark runs successfully

**Step 3: Commit**

```bash
git add test/benchmarks/test_parser.cpp
git commit -m "feat: add checkFunctionName microbenchmark"
```

---

### Task 5: Add Rule::getHash microbenchmark

**Files:**
- Modify: `test/benchmarks/test_parser.cpp`

**Step 1: Add hash calculation benchmark**

Add after CheckFunctionNameBenchmark:
```cpp
TEST_CASE("RuleHashBenchmark") {
  std::string corpus = generateRules(100);

  RuleReader r;
  REQUIRE(r.read(corpus, "benchmark"));

  const auto& rules = r.getRules();
  const auto& parser = r.getParser();
  FieldHash h;

  BENCHMARK("getHash-100rules") {
    for (const Rule& rule : rules) {
      h = rule.getHash(parser);
    }
  };

  // Verify hash is computed
  CHECK(h.to_string().size() == 32); // MD5 hex string length
}
```

**Step 2: Build and run benchmark**

Run: `meson compile -C builddir benchmarks && ./builddir/test/benchmarks "[RuleHashBenchmark]" --benchmark-samples 10`
Expected: Benchmark runs successfully

**Step 3: Commit**

```bash
git add test/benchmarks/test_parser.cpp
git commit -m "feat: add Rule::getHash microbenchmark"
```

---

### Task 6: Add separate lexer and parser scaling benchmarks

**Files:**
- Modify: `test/benchmarks/test_parser.cpp`

**Step 1: Add component-level scaling benchmarks**

Add after RuleHashBenchmark:
```cpp
TEST_CASE("LexerScalingBenchmark") {
  std::string corpus100 = generateRules(100);
  std::string corpus500 = generateRules(500);

  LlamaLexer lexer;
  size_t tokenCount = 0;

  BENCHMARK("Lexer-100rules") {
    lexer.setInput(corpus100);
    lexer.scanTokens();
    tokenCount = lexer.tokens().size();
    lexer.clear();
  };

  BENCHMARK("Lexer-500rules") {
    lexer.setInput(corpus500);
    lexer.scanTokens();
    tokenCount = lexer.tokens().size();
    lexer.clear();
  };

  CHECK(tokenCount > 0);
}

TEST_CASE("ParserScalingBenchmark") {
  std::string corpus100 = generateRules(100);
  std::string corpus500 = generateRules(500);

  // Pre-lex for parser-only timing
  LlamaLexer lexer100;
  lexer100.setInput(corpus100);
  lexer100.scanTokens();

  LlamaLexer lexer500;
  lexer500.setInput(corpus500);
  lexer500.scanTokens();

  LlamaParser parser100(corpus100, lexer100.tokens());
  LlamaParser parser500(corpus500, lexer500.tokens());

  size_t ruleCount = 0;

  BENCHMARK("Parser-100rules") {
    auto rules = parser100.parseRules(lexer100.ruleIndices());
    ruleCount = rules.size();
    parser100.resetCounters();
  };

  BENCHMARK("Parser-500rules") {
    auto rules = parser500.parseRules(lexer500.ruleIndices());
    ruleCount = rules.size();
    parser500.resetCounters();
  };

  CHECK(ruleCount > 0);
}
```

**Step 2: Build and run benchmarks**

Run: `meson compile -C builddir benchmarks && ./builddir/test/benchmarks "[LexerScalingBenchmark]" --benchmark-samples 10`
Expected: Benchmarks run successfully

**Step 3: Commit**

```bash
git add test/benchmarks/test_parser.cpp
git commit -m "feat: add lexer and parser scaling benchmarks"
```

---

### Task 7: Update test/meson.build to not require YARA for benchmarks

**Files:**
- Modify: `test/meson.build:101-114`

**Step 1: Analyze current state**

Currently benchmarks only build if YARA is found. We want parser benchmarks to always be available.

**Step 2: Split benchmark sources**

Change lines 101-114 from:
```meson
  # Benchmarks (only if YARA is available)
  if yara_dep.found()
    benchmark_sources = [
      'benchmarks/test_parser.cpp',
      'benchmarks/test_yara.cpp'
    ]

    benchmark_exe = executable('benchmarks',
      llama_common_sources + benchmark_sources,
      include_directories: [inc, include_directories('../src')],
      dependencies: llama_deps + [catch2_dep],
      build_by_default: false
    )
  endif
```

to:
```meson
  # Parser benchmarks (always available)
  parser_benchmark_sources = [
    'benchmarks/test_parser.cpp'
  ]

  parser_benchmark_exe = executable('parser_benchmarks',
    llama_common_sources + parser_benchmark_sources,
    include_directories: [inc, include_directories('../src')],
    dependencies: llama_deps + [catch2_dep],
    build_by_default: false
  )

  # YARA benchmarks (only if YARA is available)
  if yara_dep.found()
    yara_benchmark_sources = [
      'benchmarks/test_parser.cpp',
      'benchmarks/test_yara.cpp'
    ]

    yara_benchmark_exe = executable('yara_benchmarks',
      llama_common_sources + yara_benchmark_sources,
      include_directories: [inc, include_directories('../src')],
      dependencies: llama_deps + [catch2_dep],
      build_by_default: false
    )
  endif
```

**Step 3: Build to verify**

Run: `meson compile -C builddir parser_benchmarks`
Expected: Build succeeds

**Step 4: Commit**

```bash
git add test/meson.build
git commit -m "build: make parser benchmarks available without YARA dependency"
```

---

### Task 8: Final verification and baseline capture

**Step 1: Build all benchmarks**

Run: `meson compile -C builddir parser_benchmarks`
Expected: Build succeeds

**Step 2: Run full benchmark suite**

Run: `./builddir/test/parser_benchmarks --benchmark-samples 20`
Expected: All benchmarks run and produce timing data

**Step 3: Capture baseline results**

Run: `./builddir/test/parser_benchmarks --benchmark-samples 20 --reporter xml > docs/plans/baseline-benchmarks.xml`

**Step 4: Commit baseline**

```bash
git add docs/plans/baseline-benchmarks.xml
git commit -m "docs: capture baseline benchmark results"
```

---

### Task 9: Document benchmark usage

**Files:**
- Modify: `docs/plans/2025-12-23-parser-improvements-design.md`

**Step 1: Add benchmarking section**

Add to the design document:
```markdown
## Running Benchmarks

### Build
```bash
meson compile -C builddir parser_benchmarks
```

### Run all benchmarks
```bash
./builddir/test/parser_benchmarks
```

### Run specific benchmark
```bash
./builddir/test/parser_benchmarks "[LargeCorpusBenchmark]"
```

### Run with more samples for accuracy
```bash
./builddir/test/parser_benchmarks --benchmark-samples 100
```

### Export to XML for tracking
```bash
./builddir/test/parser_benchmarks --reporter xml > benchmark-results.xml
```
```

**Step 2: Commit**

```bash
git add docs/plans/2025-12-23-parser-improvements-design.md
git commit -m "docs: add benchmark usage instructions"
```
