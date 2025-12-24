# Phase 1: Error Handling Cleanup Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Clean up error handling API by removing vestigial LastError field, flattening exception hierarchy, and adding informative error reporting to file loading.

**Architecture:** RuleReader exposes errors via getLexer().errors() and getParser().errors(). The LastError field is unused. ParserError and UnexpectedInputError should both inherit directly from std::runtime_error, sharing a free function for position formatting. readRulesFromDir() should print errors with filename context.

**Tech Stack:** C++17, Catch2 for testing, Meson build system

---

### Task 1: Remove getLastError() check from test_rulereader.cpp

**Files:**
- Modify: `test/test_rulereader.cpp:29`

**Step 1: Identify the line to remove**

The test currently has:
```cpp
REQUIRE(reader.getLastError() == "");
```

This is line 29. Remove it.

**Step 2: Edit the test file**

Remove line 29 entirely. The surrounding code becomes:
```cpp
  REQUIRE(result == false);
  result = reader.read(input3, "test");
```

**Step 3: Run tests to verify they still pass**

Run: `meson test -C builddir --verbose`
Expected: All tests pass (getLastError still exists, just not tested here)

**Step 4: Commit**

```bash
git add test/test_rulereader.cpp
git commit -m "test: remove getLastError assertion from test_rulereader"
```

---

### Task 2: Remove getLastError() check from benchmark test

**Files:**
- Modify: `test/benchmarks/test_parser.cpp:100`

**Step 1: Identify the line to remove**

The benchmark test currently has:
```cpp
  CHECK(r.getLastError() == "");
```

This is line 100. Remove it.

**Step 2: Edit the benchmark file**

Remove line 100. The surrounding code becomes:
```cpp
  };
  CHECK(res);
}
```

**Step 3: Run tests to verify they still pass**

Run: `meson test -C builddir --verbose`
Expected: All tests pass

**Step 4: Commit**

```bash
git add test/benchmarks/test_parser.cpp
git commit -m "test: remove getLastError assertion from parser benchmark"
```

---

### Task 3: Remove LastError from RuleReader

**Files:**
- Modify: `include/rulereader.h:12,15,21`

**Step 1: Remove LastError.clear() from clear() method**

Change line 12 from:
```cpp
  void clear() { Rules.clear(); LastError.clear(); Parser.clear(); }
```
to:
```cpp
  void clear() { Rules.clear(); Parser.clear(); }
```

**Step 2: Remove getLastError() method**

Delete line 15:
```cpp
  const std::string& getLastError() const { return LastError; }
```

**Step 3: Remove LastError field**

Delete line 21:
```cpp
  std::string LastError;
```

**Step 4: Verify the final header looks correct**

```cpp
#pragma once

#include <string>
#include <vector>

#include "parser.h"
#include "lexer.h"

class RuleReader {
public:
  bool read(const std::string& input, const std::string& source);
  void clear() { Rules.clear(); Parser.clear(); }

  const std::vector<Rule>& getRules() const { return Rules; }
  const LlamaParser& getParser() const { return Parser; }
  const LlamaLexer& getLexer() const { return Lexer; }

private:
  std::vector<Rule> Rules;
  LlamaParser Parser;
  LlamaLexer Lexer;
};
```

**Step 5: Build and run tests**

Run: `meson compile -C builddir && meson test -C builddir --verbose`
Expected: Build succeeds, all tests pass

**Step 6: Commit**

```bash
git add include/rulereader.h
git commit -m "refactor: remove vestigial LastError from RuleReader

The field was never populated - errors are accessible via
getParser().errors() and getLexer().errors()."
```

---

### Task 4: Extract messageWithPos to free function

**Files:**
- Modify: `include/token.h:106-118`

**Step 1: Convert messageWithPos from private static to free function**

Change the UnexpectedInputError class from:
```cpp
class UnexpectedInputError : public std::runtime_error {
public:
  UnexpectedInputError(const std::string_view& message, LineCol pos)
 : std::runtime_error(messageWithPos(message, pos)) {}

private:
  static std::string messageWithPos(std::string_view errMsg, LineCol pos) {
    std::string msg(errMsg);
    msg += " at ";
    msg += pos.toString();
    return msg;
  }
};
```

to:
```cpp
inline std::string formatErrorWithPos(std::string_view errMsg, LineCol pos) {
  std::string msg(errMsg);
  msg += " at ";
  msg += pos.toString();
  return msg;
}

class UnexpectedInputError : public std::runtime_error {
public:
  UnexpectedInputError(const std::string_view& message, LineCol pos)
    : std::runtime_error(formatErrorWithPos(message, pos)) {}
};
```

**Step 2: Build and run tests**

Run: `meson compile -C builddir && meson test -C builddir --verbose`
Expected: Build succeeds, all tests pass

**Step 3: Commit**

```bash
git add include/token.h
git commit -m "refactor: extract formatErrorWithPos as free function

Prepares for flattening ParserError inheritance."
```

---

### Task 5: Make ParserError inherit from std::runtime_error directly

**Files:**
- Modify: `include/parser.h:17-20`

**Step 1: Change ParserError inheritance**

Change from:
```cpp
class ParserError : public UnexpectedInputError {
public:
  ParserError(const std::string_view& message, LineCol pos) : UnexpectedInputError(message, pos) {}
};
```

to:
```cpp
class ParserError : public std::runtime_error {
public:
  ParserError(const std::string_view& message, LineCol pos)
    : std::runtime_error(formatErrorWithPos(message, pos)) {}
};
```

**Step 2: Build and run tests**

Run: `meson compile -C builddir && meson test -C builddir --verbose`
Expected: Build succeeds, all tests pass

**Step 3: Commit**

```bash
git add include/parser.h
git commit -m "refactor: make ParserError inherit from std::runtime_error

Both ParserError and UnexpectedInputError now independently inherit
from std::runtime_error and share formatErrorWithPos for formatting."
```

---

### Task 6: Add getReader() accessor to LlamaRuleEngine

**Files:**
- Modify: `include/ruleengine.h:25`

**Step 1: Add the accessor method**

After line 25 (after setPatternToRuleId), add:
```cpp
  const RuleReader& getReader() const { return Reader; }
```

The public section should now include:
```cpp
  const std::vector<std::string>& patternToRuleId() const { return PatternToRuleId; }
  void setPatternToRuleId(const std::vector<std::string>& patternToRuleId) { PatternToRuleId = patternToRuleId; }
  const RuleReader& getReader() const { return Reader; }
```

**Step 2: Build to verify**

Run: `meson compile -C builddir`
Expected: Build succeeds

**Step 3: Commit**

```bash
git add include/ruleengine.h
git commit -m "feat: add getReader() accessor to LlamaRuleEngine

Enables access to lexer/parser errors for error reporting."
```

---

### Task 7: Add error reporting to readRulesFromDir()

**Files:**
- Modify: `src/llama.cpp:102-112`

**Step 1: Update readRulesFromDir to print errors**

Change from:
```cpp
bool readRulesFromDir(std::shared_ptr<LlamaRuleEngine> engine, const std::string& path) {
  std::filesystem::path ruleDir{path};
  bool ret = false;

  for (const auto& file : std::filesystem::directory_iterator{ruleDir}) {
    // don't exit early if there's an error because we want to give users all errors possible
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
  bool ret = true;

  for (const auto& file : std::filesystem::directory_iterator{ruleDir}) {
    std::string filePath = file.path().string();
    if (!engine->read(readfile(filePath), filePath)) {
      ret = false;
      // Print lexer errors with filename context
      for (const auto& err : engine->getReader().getLexer().errors()) {
        std::cerr << filePath << ": " << err.what() << '\n';
      }
      // Print parser errors with filename context
      for (const auto& err : engine->getReader().getParser().errors()) {
        std::cerr << filePath << ": " << err.what() << '\n';
      }
    }
  }
  return ret;
}
```

**Note:** Changed `ret = false` initialization to `ret = true` and logic to `ret = false` on failure, which is clearer. The `|=` was confusing because `true |= false` stays true but the semantics suggested accumulating success.

**Step 2: Build and verify**

Run: `meson compile -C builddir`
Expected: Build succeeds

**Step 3: Manual test with a bad rule file**

Create a temporary bad rule file and test:
```bash
echo "rule {}" > /tmp/bad.llama
# Run llama with --rule-dir /tmp and verify error output includes filename
```

**Step 4: Commit**

```bash
git add src/llama.cpp
git commit -m "feat: add error reporting to readRulesFromDir

Prints lexer and parser errors with filename context to stderr,
giving users visibility into which rule file has problems."
```

---

### Task 8: Final verification

**Step 1: Run full test suite**

Run: `meson test -C builddir --verbose`
Expected: All tests pass

**Step 2: Review changes**

Run: `git log --oneline -8`
Expected: See the 7 commits from this phase

**Step 3: Verify no compiler warnings**

Run: `meson compile -C builddir 2>&1 | grep -i warning || echo "No warnings"`
Expected: No new warnings related to our changes
