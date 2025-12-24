# Phase 4: Global Optimizations Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Implement global optimizations that require broader code changes: arena allocation for AST nodes (eliminates per-node heap allocations) and parallel file loading (utilizes multiple cores for rule loading).

**Architecture:** Arena allocation stores nodes in vectors owned by the parser, using indices instead of shared_ptr. Parallel loading uses boost::asio::thread_pool (already used in codebase) with separate RuleReader instances per file.

**Tech Stack:** C++17, boost::asio::thread_pool, Catch2 benchmarking, Meson build system

---

## Optimization 1: Arena Allocation for AST Nodes

This is a significant refactoring that touches the parser, querybuilder, and rule structures. We'll do it incrementally.

### Task 1: Add NodeIndex type alias and INVALID_NODE constant

**Files:**
- Modify: `include/parser.h`

**Step 1: Add type alias before Node struct**

Add after line 23 (after `struct Atom {};`):
```cpp
// Index into node arenas, SIZE_MAX represents null/invalid
using NodeIndex = size_t;
constexpr NodeIndex INVALID_NODE = SIZE_MAX;
```

**Step 2: Build to verify**

Run: `meson compile -C builddir`
Expected: Build succeeds

**Step 3: Commit**

```bash
git add include/parser.h
git commit -m "feat: add NodeIndex type alias for arena allocation"
```

---

### Task 2: Add node arenas to LlamaParser

**Files:**
- Modify: `include/parser.h` (LlamaParser class, around line 366)

**Step 1: Add arena vectors to LlamaParser**

In the LlamaParser class, after the existing public members (around line 370), add:
```cpp
  // Node arenas for allocation
  std::vector<BoolNode> BoolNodes;
  std::vector<FuncNode> FuncNodes;
  std::vector<PropertyNode> PropNodes;

  // Allocate a node and return its index
  NodeIndex allocBoolNode() {
    BoolNodes.emplace_back();
    return BoolNodes.size() - 1;
  }

  NodeIndex allocFuncNode() {
    FuncNodes.emplace_back();
    return FuncNodes.size() - 1;
  }

  NodeIndex allocPropNode() {
    PropNodes.emplace_back();
    return PropNodes.size() - 1;
  }

  // Access nodes by index
  BoolNode& getBoolNode(NodeIndex idx) { return BoolNodes[idx]; }
  const BoolNode& getBoolNode(NodeIndex idx) const { return BoolNodes[idx]; }
  FuncNode& getFuncNode(NodeIndex idx) { return FuncNodes[idx]; }
  const FuncNode& getFuncNode(NodeIndex idx) const { return FuncNodes[idx]; }
  PropertyNode& getPropNode(NodeIndex idx) { return PropNodes[idx]; }
  const PropertyNode& getPropNode(NodeIndex idx) const { return PropNodes[idx]; }
```

**Step 2: Update clear() method**

The `clear()` method in LlamaParser needs to also clear the arenas. In `src/parser.cpp`, update the `clear()` function:
```cpp
void LlamaParser::clear() {
  Tokens.clear();
  Input.clear();
  Errors.clear();
  BoolNodes.clear();
  FuncNodes.clear();
  PropNodes.clear();
  resetCounters();
}
```

**Step 3: Build to verify**

Run: `meson compile -C builddir`
Expected: Build succeeds

**Step 4: Commit**

```bash
git add include/parser.h src/parser.cpp
git commit -m "feat: add node arenas to LlamaParser"
```

---

### Task 3: Modify Node struct to use indices instead of shared_ptr

**Files:**
- Modify: `include/parser.h` (Node struct, around line 34)

**Step 1: Change Node to use indices**

Change from:
```cpp
struct Node {
  virtual ~Node() = default;

  Node(NodeType type) : Type(type) {}
  Node() = default;

  NodeType Type;
  std::shared_ptr<Node> Left;
  std::shared_ptr<Node> Right;
};
```

to:
```cpp
struct Node {
  Node(NodeType type) : Type(type) {}
  Node() = default;

  NodeType Type = NodeType::BOOL;
  NodeIndex Left = INVALID_NODE;
  NodeIndex Right = INVALID_NODE;
};
```

Note: We remove the virtual destructor since nodes are now stored in typed vectors and don't need polymorphic deletion.

**Step 2: Build (expect errors)**

Run: `meson compile -C builddir 2>&1 | head -50`
Expected: Compilation errors from code still using shared_ptr

**Step 3: Commit this intermediate state**

```bash
git add include/parser.h
git commit -m "WIP: change Node to use indices instead of shared_ptr"
```

---

### Task 4: Update Rule struct to use NodeIndex

**Files:**
- Modify: `include/parser.h` (Rule struct, around line 159)

**Step 1: Change Rule to use indices**

Change from:
```cpp
struct Rule {
  // ...
  std::shared_ptr<Node> Signature;
  std::shared_ptr<Node> FileMetadata;
  // ...
};
```

to:
```cpp
struct Rule {
  // ...
  NodeIndex Signature = INVALID_NODE;
  NodeIndex FileMetadata = INVALID_NODE;
  // ...
};
```

**Step 2: Build (expect errors)**

Run: `meson compile -C builddir 2>&1 | head -50`
Expected: Compilation errors continue

**Step 3: Commit**

```bash
git add include/parser.h
git commit -m "WIP: change Rule to use NodeIndex"
```

---

### Task 5: Update GrepSection to use NodeIndex

**Files:**
- Modify: `include/parser.h` (GrepSection struct, around line 135)

**Step 1: Change Condition to NodeIndex**

Change from:
```cpp
struct GrepSection {
  PatternSection Patterns;
  std::shared_ptr<Node> Condition;
};
```

to:
```cpp
struct GrepSection {
  PatternSection Patterns;
  NodeIndex Condition = INVALID_NODE;
};
```

**Step 2: Commit**

```bash
git add include/parser.h
git commit -m "WIP: change GrepSection to use NodeIndex"
```

---

### Task 6: Update parser expression functions

**Files:**
- Modify: `src/parser.cpp`

**Step 1: Update parseFactor**

Change from:
```cpp
std::shared_ptr<Node> LlamaParser::parseFactor(LlamaTokenType section) {
  std::shared_ptr<Node> node;
  if (matchAny(LlamaTokenType::OPEN_PAREN)) {
    node = parseExpr(section);
    expect(LlamaTokenType::CLOSE_PAREN);
  }
  else if (section == LlamaTokenType::FILE_METADATA || section == LlamaTokenType::SIGNATURE) {
    node = std::make_shared<PropertyNode>(parseProperty(section));;
  }
  else if (checkFunctionName()) {
    if (section != LlamaTokenType::CONDITION) throw ParserError("Invalid property in section", previous().Pos);
    node = std::make_shared<FuncNode>(parseFuncCall());
  }
  else {
    throw ParserError("Expected function call or signature definition", peek().Pos);
  }
  return node;
}
```

to:
```cpp
NodeIndex LlamaParser::parseFactor(LlamaTokenType section) {
  if (matchAny(LlamaTokenType::OPEN_PAREN)) {
    NodeIndex node = parseExpr(section);
    expect(LlamaTokenType::CLOSE_PAREN);
    return node;
  }
  else if (section == LlamaTokenType::FILE_METADATA || section == LlamaTokenType::SIGNATURE) {
    NodeIndex idx = allocPropNode();
    PropNodes[idx] = PropertyNode(parseProperty(section));
    return idx;
  }
  else if (checkFunctionName()) {
    if (section != LlamaTokenType::CONDITION) throw ParserError("Invalid property in section", previous().Pos);
    NodeIndex idx = allocFuncNode();
    FuncNodes[idx] = FuncNode(parseFuncCall());
    return idx;
  }
  else {
    throw ParserError("Expected function call or signature definition", peek().Pos);
  }
}
```

**Step 2: Update parseTerm**

Change from:
```cpp
std::shared_ptr<Node> LlamaParser::parseTerm(LlamaTokenType section) {
  std::shared_ptr<Node> left = parseFactor(section);

  while (matchAny(LlamaTokenType::AND)) {
    std::shared_ptr<BoolNode> node = std::make_shared<BoolNode>();
    node->Operation = BoolNode::Op::AND;
    node->Type = NodeType::BOOL;
    node->Left = left;
    node->Right = parseFactor(section);
    left = node;
  }
  return left;
}
```

to:
```cpp
NodeIndex LlamaParser::parseTerm(LlamaTokenType section) {
  NodeIndex left = parseFactor(section);

  while (matchAny(LlamaTokenType::AND)) {
    NodeIndex idx = allocBoolNode();
    BoolNodes[idx].Operation = BoolNode::Op::AND;
    BoolNodes[idx].Type = NodeType::BOOL;
    BoolNodes[idx].Left = left;
    BoolNodes[idx].Right = parseFactor(section);
    left = idx;
  }
  return left;
}
```

**Step 3: Update parseExpr**

Change from:
```cpp
std::shared_ptr<Node> LlamaParser::parseExpr(LlamaTokenType section) {
  std::shared_ptr<Node> left = parseTerm(section);

  while (matchAny(LlamaTokenType::OR)) {
    std::shared_ptr<BoolNode> node = std::make_shared<BoolNode>();
    node->Operation = BoolNode::Op::OR;
    node->Type = NodeType::BOOL;
    node->Left = left;
    node->Right = parseTerm(section);
    left = node;
  }
  return left;
}
```

to:
```cpp
NodeIndex LlamaParser::parseExpr(LlamaTokenType section) {
  NodeIndex left = parseTerm(section);

  while (matchAny(LlamaTokenType::OR)) {
    NodeIndex idx = allocBoolNode();
    BoolNodes[idx].Operation = BoolNode::Op::OR;
    BoolNodes[idx].Type = NodeType::BOOL;
    BoolNodes[idx].Left = left;
    BoolNodes[idx].Right = parseTerm(section);
    left = idx;
  }
  return left;
}
```

**Step 4: Commit**

```bash
git add src/parser.cpp
git commit -m "WIP: update parser expression functions for arena allocation"
```

---

### Task 7: Update parser.h declarations

**Files:**
- Modify: `include/parser.h` (function declarations around line 348-350)

**Step 1: Update declarations**

Change from:
```cpp
  std::shared_ptr<Node> parseFactor(LlamaTokenType section);
  std::shared_ptr<Node> parseTerm(LlamaTokenType section);
  std::shared_ptr<Node> parseExpr(LlamaTokenType section);
```

to:
```cpp
  NodeIndex parseFactor(LlamaTokenType section);
  NodeIndex parseTerm(LlamaTokenType section);
  NodeIndex parseExpr(LlamaTokenType section);
```

**Step 2: Commit**

```bash
git add include/parser.h
git commit -m "WIP: update parser function declarations"
```

---

### Task 8: Update QueryBuilder to use indices

**Files:**
- Modify: `include/querybuilder.h`
- Modify: `src/querybuilder.cpp`

**Step 1: Update querybuilder.h declarations**

Change from:
```cpp
  std::string buildSqlClause(std::shared_ptr<Node> n);
  std::string buildSqlClause(std::shared_ptr<PropertyNode> pn);
  std::string buildSqlClause(std::shared_ptr<BoolNode> bn);
```

to:
```cpp
  std::string buildSqlClause(NodeIndex idx, NodeType type);
  std::string buildSqlClause(const PropertyNode& pn);
  std::string buildSqlClause(const BoolNode& bn, NodeIndex leftIdx, NodeIndex rightIdx);
```

**Step 2: Update querybuilder.cpp implementations**

Change `buildSqlClause` implementations to work with indices and references instead of shared_ptr. The QueryBuilder already has a reference to Parser, so it can access nodes via indices.

```cpp
std::string QueryBuilder::buildSqlClause(NodeIndex idx, NodeType type) {
  std::string clause;
  switch (type) {
    case NodeType::PROP: {
      clause = buildSqlClause(Parser.getPropNode(idx));
      break;
    }
    case NodeType::BOOL: {
      const auto& bn = Parser.getBoolNode(idx);
      clause = buildSqlClause(bn, bn.Left, bn.Right);
      break;
    }
    default: {
      throw std::runtime_error("Invalid node type " + std::to_string(static_cast<int>(type)));
    }
  }
  return clause;
}

std::string QueryBuilder::buildSqlClause(const PropertyNode& pn) {
  std::string clause;
  std::string_view propertyName = Parser.lexemeAt(pn.Value.Name);
  clause += FileMetadataPropertySqlLookup.find(propertyName)->second;
  clause += " ";
  clause += Parser.lexemeAt(pn.Value.Op);
  clause += " ";
  std::string_view val = Parser.lexemeAt(pn.Value.Val);
  if (Parser.Tokens[pn.Value.Val].Type == LlamaTokenType::DOUBLE_QUOTED_STRING) {
    clause += "'";
    clause += val;
    clause += "'";
  }
  else {
    clause += val;
  }
  return clause;
}

std::string QueryBuilder::buildSqlClause(const BoolNode& bn, NodeIndex leftIdx, NodeIndex rightIdx) {
  std::string clause = "(";
  // Need to determine type of left/right nodes - this requires storing type info
  // For now, we need to look it up. This is a design consideration.
  clause += buildSqlClause(leftIdx, /* need type */);
  clause += bn.Operation == BoolNode::Op::AND ? " AND " : " OR ";
  clause += buildSqlClause(rightIdx, /* need type */);
  clause += ")";
  return clause;
}
```

**Note:** This reveals a design issue: when traversing the tree via indices, we need to know the node type. Options:
1. Store the NodeType alongside each NodeIndex in parent nodes
2. Have a unified node vector with a discriminator
3. Keep the current polymorphic design but with an arena allocator

**Step 3: Design decision needed**

Given the complexity of tracking types through indices, consider storing `NodeType` alongside each child index:

```cpp
struct NodeRef {
  NodeIndex Index = INVALID_NODE;
  NodeType Type = NodeType::BOOL;
};

struct Node {
  NodeType Type = NodeType::BOOL;
  NodeRef Left;
  NodeRef Right;
};
```

This adds 4 bytes overhead per reference but eliminates the type lookup problem.

**Step 4: Commit work in progress**

```bash
git add include/querybuilder.h src/querybuilder.cpp
git commit -m "WIP: update QueryBuilder for arena allocation (incomplete)"
```

---

### Task 9: Complete the arena allocation refactoring

**Files:**
- Modify: All files with compilation errors

This task involves fixing all remaining compilation errors from the arena allocation changes. Work through each error systematically:

1. Update any code that creates nodes with `make_shared` to use arena allocation
2. Update any code that accesses nodes via `->` to use index lookups
3. Update tests that construct or verify nodes

**Step 1: Build and fix each error**

Run: `meson compile -C builddir 2>&1`
Fix each error one at a time.

**Step 2: Run tests**

Run: `meson test -C builddir --verbose`
Expected: All tests pass

**Step 3: Commit**

```bash
git add -u
git commit -m "refactor: complete arena allocation for AST nodes"
```

---

### Task 10: Add A/B benchmark for arena allocation

**Files:**
- Modify: `test/benchmarks/test_parser.cpp`

**Step 1: Add benchmark comparing before/after**

Since we can't easily have both implementations, compare against baseline captured earlier:

```cpp
TEST_CASE("ArenaAllocationBenchmark") {
  std::string corpus = generateRules(500);

  RuleReader r;
  size_t ruleCount = 0;

  BENCHMARK("Parser-500rules-arena") {
    r.read(corpus, "benchmark");
    ruleCount = r.getRules().size();
    r.clear();
  };

  CHECK(ruleCount == 500);
}
```

**Step 2: Run and compare to Phase 2 baseline**

Run: `./builddir/test/parser_benchmarks "[ArenaAllocationBenchmark]" --benchmark-samples 20`
Compare to baseline from Phase 2.

**Step 3: Commit**

```bash
git add test/benchmarks/test_parser.cpp
git commit -m "test: add benchmark for arena allocation validation"
```

---

## Optimization 2: Parallel File Loading

### Task 11: Add A/B benchmark for parallel loading

**Files:**
- Create: `test/benchmarks/parallel_load_benchmark.cpp`
- Modify: `test/meson.build`

**Step 1: Create benchmark test file**

This requires a directory of rule files. For testing, we can create temporary files:

```cpp
#include <catch2/benchmark/catch_benchmark_all.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "rule_generator.h"
#include "llama.h"
#include "ruleengine.h"

namespace fs = std::filesystem;

// Helper to create temporary rule files
class TempRuleDir {
public:
  TempRuleDir(size_t fileCount, size_t rulesPerFile) {
    Dir = fs::temp_directory_path() / "llama_bench_rules";
    fs::create_directories(Dir);

    for (size_t i = 0; i < fileCount; ++i) {
      std::string rules = generateRules(rulesPerFile);
      std::ofstream out(Dir / ("rules_" + std::to_string(i) + ".llama"));
      out << rules;
    }
  }

  ~TempRuleDir() {
    fs::remove_all(Dir);
  }

  const fs::path& path() const { return Dir; }

private:
  fs::path Dir;
};

TEST_CASE("ParallelLoadBenchmark") {
  TempRuleDir tempDir(20, 50);  // 20 files, 50 rules each = 1000 rules

  auto engine = std::make_shared<LlamaRuleEngine>();

  BENCHMARK("readRulesFromDir-sequential") {
    engine = std::make_shared<LlamaRuleEngine>();
    readRulesFromDir(engine, tempDir.path().string());
    return engine->numRulesRead();
  };
}
```

**Step 2: Update meson.build to include new benchmark**

Add the new file to `parser_benchmark_sources`.

**Step 3: Build and run**

Run: `meson compile -C builddir parser_benchmarks && ./builddir/test/parser_benchmarks "[ParallelLoadBenchmark]" --benchmark-samples 5`

**Step 4: Commit**

```bash
git add test/benchmarks/parallel_load_benchmark.cpp test/meson.build
git commit -m "test: add benchmark for parallel file loading"
```

---

### Task 12: Implement parallel readRulesFromDir

**Files:**
- Modify: `src/llama.cpp`
- Modify: `include/llama.h`

**Step 1: Add necessary includes**

In `src/llama.cpp`, ensure these includes are present:
```cpp
#include "boost_asio.h"
#include "easyfut.h"
#include <mutex>
```

**Step 2: Implement parallel version**

Change `readRulesFromDir` from:
```cpp
bool readRulesFromDir(std::shared_ptr<LlamaRuleEngine> engine, const std::string& path) {
  std::filesystem::path ruleDir{path};
  bool ret = false;

  for (const auto& file : std::filesystem::directory_iterator{ruleDir}) {
    std::string filePath = file.path().string();
    ret |= engine->read(readfile(filePath), filePath);
  }
  return ret;
}
```

to:
```cpp
bool readRulesFromDir(std::shared_ptr<LlamaRuleEngine> engine, const std::string& path) {
  std::filesystem::path ruleDir{path};

  // Collect all file paths first
  std::vector<std::string> filePaths;
  for (const auto& file : std::filesystem::directory_iterator{ruleDir}) {
    if (file.is_regular_file()) {
      filePaths.push_back(file.path().string());
    }
  }

  if (filePaths.empty()) {
    return true;
  }

  // Create thread pool for parallel loading
  const size_t threadCount = std::min(
    static_cast<size_t>(std::thread::hardware_concurrency()),
    filePaths.size()
  );
  boost::asio::thread_pool pool(threadCount);

  // Structure to hold results from each file
  struct FileResult {
    std::string FilePath;
    std::string Content;
    RuleReader Reader;
    bool Success = false;
  };

  std::vector<FileResult> results(filePaths.size());
  std::vector<easy_fut<bool>> futures;

  // Launch parallel reads
  for (size_t i = 0; i < filePaths.size(); ++i) {
    results[i].FilePath = filePaths[i];
    futures.push_back(make_future(pool, [&results, i]() {
      results[i].Content = readfile(results[i].FilePath);
      results[i].Success = results[i].Reader.read(results[i].Content, results[i].FilePath);
      return results[i].Success;
    }));
  }

  // Wait for all to complete
  pool.join();

  // Merge results and report errors
  bool allSuccess = true;
  for (size_t i = 0; i < results.size(); ++i) {
    futures[i].get();  // Ensure completion

    if (!results[i].Success) {
      allSuccess = false;
      // Print lexer errors
      for (const auto& err : results[i].Reader.getLexer().errors()) {
        std::cerr << results[i].FilePath << ": " << err.what() << '\n';
      }
      // Print parser errors
      for (const auto& err : results[i].Reader.getParser().errors()) {
        std::cerr << results[i].FilePath << ": " << err.what() << '\n';
      }
    }

    // Merge rules into engine
    // Note: LlamaRuleEngine::read() copies rules, we need a way to add rules directly
    // For now, we can re-read from the already-parsed reader
    for (const Rule& rule : results[i].Reader.getRules()) {
      // Need to add a method to LlamaRuleEngine to accept pre-parsed rules
    }
  }

  return allSuccess;
}
```

**Step 3: Add addRules method to LlamaRuleEngine**

In `include/ruleengine.h`, add:
```cpp
  void addRules(const std::vector<Rule>& rules, const std::string& input);
```

In `src/ruleengine.cpp`, implement:
```cpp
void LlamaRuleEngine::addRules(const std::vector<Rule>& rules, const std::string& input) {
  // Store input to ensure string_view lifetime
  // Note: This needs careful handling of string_view lifetimes
  // Each file's rules reference that file's input string
}
```

**Design note:** The string_view lifetime issue means we need to keep each file's content alive. The parallel implementation should store the content strings and pass them to the engine.

**Step 4: Build and test**

Run: `meson compile -C builddir && meson test -C builddir --verbose`

**Step 5: Run benchmark comparison**

Run: `./builddir/test/parser_benchmarks "[ParallelLoadBenchmark]" --benchmark-samples 5`

**Step 6: Commit**

```bash
git add src/llama.cpp include/llama.h include/ruleengine.h src/ruleengine.cpp
git commit -m "perf: implement parallel file loading for readRulesFromDir

Uses boost::asio::thread_pool to parse multiple rule files concurrently.
Each file gets its own RuleReader instance. Results are merged after
all files complete."
```

---

### Task 13: Final verification

**Step 1: Run full test suite**

Run: `meson test -C builddir --verbose`
Expected: All tests pass

**Step 2: Run all benchmarks**

Run: `./builddir/test/parser_benchmarks --benchmark-samples 20`

**Step 3: Capture final results**

Run: `./builddir/test/parser_benchmarks --benchmark-samples 20 --reporter xml > docs/plans/phase4-benchmark-results.xml`

**Step 4: Commit**

```bash
git add docs/plans/phase4-benchmark-results.xml
git commit -m "docs: capture Phase 4 optimization benchmark results"
```

---

## Implementation Notes

### Arena Allocation Complexity

The arena allocation refactoring is complex because:
1. Node types need to be tracked when traversing the tree
2. QueryBuilder needs to access nodes through the parser
3. Tests that construct nodes directly need updating

Consider a phased approach:
- First, add arenas alongside existing shared_ptr (both work)
- Migrate code incrementally
- Remove shared_ptr once all code uses arenas

### Parallel Loading String Lifetime

The parallel loading implementation must handle string_view lifetime carefully:
- Each RuleReader's tokens contain string_views into the input
- The input string must outlive the rules
- Store input strings in the engine alongside the rules

### Alternative: std::pmr Allocators

C++17's polymorphic memory resources could provide arena allocation without changing the pointer types:
```cpp
std::pmr::monotonic_buffer_resource arena;
std::pmr::polymorphic_allocator<Node> alloc(&arena);
auto node = std::allocate_shared<Node>(alloc, ...);
```

This is less intrusive but doesn't eliminate shared_ptr overhead entirely.
