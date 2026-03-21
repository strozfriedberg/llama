# File Signature Analysis Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Wire file signature detection into Llama's processing pipeline so that signatures are detected via Lightgrep, stored in the database, and evaluated in rules.

**Architecture:** Rewrite `FileSigAnalyzer` to use `ReadSeek` and pattern-only Lightgrep matching (dropping the Check subsystem). Add two DuckDB tables (`signatures` and `file_signatures`). Integrate into the processing pipeline: compile patterns in `init()`, detect signatures in `Processor::process()`, evaluate `signature:` rule sections via SQL JOINs in `QueryBuilder`.

**Tech Stack:** C++20, Lightgrep, DuckDB (C API), Catch2, Boost (Outcome, ASIO), jsoncons

---

### Task 1: Create `ducksig.h` with DB record structs

**Files:**
- Create: `include/ducksig.h`
- Reference: `include/duckhash.h` (follow this pattern exactly)
- Reference: `include/llamaduck.h` (for `DBBatch` template)

**Step 1: Write the failing test**

Add to `test/test_duckdb.cpp`:

```cpp
#include "ducksig.h"

TEST_CASE("SigRecBatch") {
  LlamaDB db;
  LlamaDBConnection conn(db);
  DBType<SigRec>::createTable(conn.get(), "signatures");

  LlamaDBAppender appender(conn.get(), "signatures");
  SigBatch batch;
  batch.add(SigRec{"id-1", "PDF", "Portable Document Format"});
  batch.add(SigRec{"id-2", "JPEG", "JPEG image"});
  REQUIRE(batch.size() == 2);
  batch.copyToDB(appender.get());
  appender.flush();

  duckdb_result result;
  duckdb_query(conn.get(), "SELECT count(*) FROM signatures", &result);
  REQUIRE(duckdb_value_int64(&result, 0, 0) == 2);
  duckdb_destroy_result(&result);
}

TEST_CASE("FileSigResultBatch") {
  LlamaDB db;
  LlamaDBConnection conn(db);
  DBType<FileSigResult>::createTable(conn.get(), "file_signatures");

  LlamaDBAppender appender(conn.get(), "file_signatures");
  FileSigBatch batch;
  batch.add(FileSigResult{"abc123hash", "sig-id-1"});
  batch.add(FileSigResult{"abc123hash", "sig-id-2"});
  REQUIRE(batch.size() == 2);
  batch.copyToDB(appender.get());
  appender.flush();

  duckdb_result result;
  duckdb_query(conn.get(), "SELECT count(*) FROM file_signatures", &result);
  REQUIRE(duckdb_value_int64(&result, 0, 0) == 2);
  duckdb_destroy_result(&result);
}
```

**Step 2: Run test to verify it fails**

Run: `meson compile -C builddir && builddir/llama_test "[SigRecBatch]"`
Expected: Compilation fails — `ducksig.h` does not exist.

**Step 3: Write minimal implementation**

Create `include/ducksig.h`:

```cpp
#pragma once

#include "llamaduck.h"

struct SigRec {
  static constexpr auto ColNames = {"Id",
                                    "Name",
                                    "Description"};
  std::string Id;
  std::string Name;
  std::string Description;
};

using SigBatch = DBBatch<SigRec>;

struct FileSigResult {
  static constexpr auto ColNames = {"FileHash",
                                    "SigId"};
  std::string FileHash;
  std::string SigId;
};

using FileSigBatch = DBBatch<FileSigResult>;
```

**Step 4: Run test to verify it passes**

Run: `meson compile -C builddir && builddir/llama_test "[SigRecBatch]" "[FileSigResultBatch]"`
Expected: PASS

**Step 5: Commit**

```bash
git add include/ducksig.h test/test_duckdb.cpp
git commit -m "Add ducksig.h with SigRec and FileSigResult DB record structs"
```

---

### Task 2: Simplify `Magic` struct and `readMagics()`

Strip `Check`, `CompareType`, `OffsetType`, and related types from `filesignatures.h`. Simplify `readMagics()` to skip entries without patterns and ignore checks. Remove all Check-related functions from `filesignatures.cpp`.

**Files:**
- Modify: `include/filesignatures.h`
- Modify: `src/filesignatures.cpp`
- Modify: `test/test_filesignatures.cpp`

**Step 1: Write the failing test**

Replace the existing tests in `test/test_filesignatures.cpp` that depend on Check infrastructure. The "Parsing offsets" test directly tests Check/Offset behavior which is being removed. The "Compare with verified signatures" test uses `directory_entry` and the old interface. Keep "Compare with verified data" (`getPatternLength`) as-is since it doesn't depend on Check.

Write a new test that verifies `readMagics()` skips entries without patterns:

```cpp
TEST_CASE("readMagics skips entries without patterns") {
  auto result = FileSignatures::FileSigAnalyzer::readMagics("./magics.json");
  REQUIRE(result.has_value());
  auto& magics = result.value();
  for (const auto& m : magics) {
    REQUIRE_FALSE(m->Pattern.empty());
  }
}
```

**Step 2: Run test to verify it fails**

Run: `meson compile -C builddir && builddir/llama_test "readMagics skips entries without patterns"`
Expected: FAIL — current `readMagics()` includes entries without patterns.

**Step 3: Implement the simplification**

In `include/filesignatures.h`:
- Remove: `CompareType` enum, `OffsetType` struct, `Check` struct (and `ChecksType` inside `Magic`), `parseOffset()`, `char2uint8()`, `str2bin()` declarations
- Remove: `Checks` field from `Magic`
- Remove: `SignatureDict`, `SignatureList`, `getBuf()`, `doCheck()` from `FileSigAnalyzer`
- Keep: `Magic` (with Pattern, FixedString, CaseInsensitive, Encodings, Name, Description, Id, Tags, Extensions, `getPatternLength()`), `LightGrep`, `lgSearch()`, `lgCallbackfn()`, `readMagics()`, `getPatternLength()` free function, `startsWith()`

In `src/filesignatures.cpp`:
- Remove: `parse_compare_type()`, `iequals()`, `Comparator`, `Magic::Check::compare()`, `parseOffset()`, `char2uint8()`, `str2bin()`, `readChecks()`, `FileSigAnalyzer::getBuf()`, `FileSigAnalyzer::doCheck()`
- Modify `readMagics()`: skip entries where `Pattern.empty()`, remove call to `readChecks()`
- Remove extension-based and brute-force fallback logic from `getSignature()`

In `test/test_filesignatures.cpp`:
- Remove "Parsing offsets" test (tests Check/Offset infrastructure being removed)
- Remove "Compare with verified signatures" test (uses `directory_entry` and old interface — will be replaced in Task 3)
- Keep "Compare with verified data" (`getPatternLength`) test

**Step 4: Run tests to verify they pass**

Run: `meson compile -C builddir && builddir/llama_test "[getPatternLength]" "readMagics skips entries without patterns"`
Expected: PASS

**Step 5: Commit**

```bash
git add include/filesignatures.h src/filesignatures.cpp test/test_filesignatures.cpp
git commit -m "Simplify Magic struct: remove Check subsystem and related code"
```

---

### Task 3: Rewrite `FileSigAnalyzer` to use `ReadSeek` and return all matches

Change the constructor to accept pre-parsed `MagicsType` and a shared `LG_HPROGRAM`. Create a per-instance `LG_HCONTEXT`. Change `getSignature()` to accept `ReadSeek&` and return all matches.

**Files:**
- Modify: `include/filesignatures.h`
- Modify: `src/filesignatures.cpp`
- Modify: `test/test_filesignatures.cpp`

**Step 1: Write the failing test**

```cpp
#include "readseek_impl.h"

TEST_CASE("FileSigAnalyzer detects PDF via ReadSeek") {
  // PDF magic: %PDF-
  std::vector<uint8_t> pdfBytes = {0x25, 0x50, 0x44, 0x46, 0x2D};
  ReadSeekBuf rs(pdfBytes);

  auto magicsResult = FileSignatures::FileSigAnalyzer::readMagics("./magics.json");
  REQUIRE(magicsResult.has_value());
  auto& magics = magicsResult.value();

  FileSignatures::FileSigAnalyzer analyzer(magics);
  std::vector<FileSignatures::MagicPtr> results;
  auto ok = analyzer.getSignatures(rs, results);
  REQUIRE(ok.has_value());
  REQUIRE_FALSE(results.empty());
  // At least one match should have "PDF" in its name
  bool foundPdf = false;
  for (const auto& r : results) {
    if (r->Name.find("PDF") != std::string::npos) {
      foundPdf = true;
      break;
    }
  }
  REQUIRE(foundPdf);
}

TEST_CASE("FileSigAnalyzer returns empty for unrecognized data") {
  std::vector<uint8_t> garbage = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
  ReadSeekBuf rs(garbage);

  auto magicsResult = FileSignatures::FileSigAnalyzer::readMagics("./magics.json");
  REQUIRE(magicsResult.has_value());
  auto& magics = magicsResult.value();

  FileSignatures::FileSigAnalyzer analyzer(magics);
  std::vector<FileSignatures::MagicPtr> results;
  auto ok = analyzer.getSignatures(rs, results);
  REQUIRE(ok.has_value());
  REQUIRE(results.empty());
}
```

**Step 2: Run test to verify it fails**

Run: `meson compile -C builddir && builddir/llama_test "FileSigAnalyzer detects PDF via ReadSeek"`
Expected: Compilation fails — new constructor and `getSignatures()` method don't exist yet.

**Step 3: Implement the new interface**

In `include/filesignatures.h`:

Replace the `FileSigAnalyzer` class:

```cpp
class FileSigAnalyzer {
  MagicsType Magics;
  LightGrep Lg;
  std::vector<uint8_t> ReadBuf;

  // Each instance has its own LG context for thread safety
  // (LG program is compiled once and shared, but search context is per-instance)

  expected<bool> lgSearch(const uint8_t* start, const uint8_t* end,
                          std::vector<MagicPtr>& results) const;
  static void lgCallbackfn(void* userData, const LG_SearchHit* const hit);

public:
  // Takes pre-parsed signatures. Compiles Lightgrep program and allocates read buffer.
  FileSigAnalyzer(const MagicsType& magics);

  static expected<MagicsType> readMagics(std::string_view path);

  // Detect signatures in a ReadSeek stream. Populates results with all matches.
  expected<bool> getSignatures(ReadSeek& rs, std::vector<MagicPtr>& results) const;
};
```

In `src/filesignatures.cpp`:

- New constructor takes `MagicsType`, sorts by pattern length, calls `Lg.setup()`, sizes `ReadBuf`.
- `getSignatures()`: seeks to 0, reads `ReadBuf.size()` bytes from `ReadSeek`, calls `lgSearch()`.
- `lgCallbackfn()`: collects all hit indices into a vector (not just the minimum).
- `lgSearch()`: iterates collected indices and pushes corresponding `MagicPtr`s into results.

**Step 4: Run tests to verify they pass**

Run: `meson compile -C builddir && builddir/llama_test "[testSignatures]" "[getPatternLength]" "readMagics skips"`
Expected: PASS (all file signature tests)

**Step 5: Commit**

```bash
git add include/filesignatures.h src/filesignatures.cpp test/test_filesignatures.cpp
git commit -m "Rewrite FileSigAnalyzer to use ReadSeek and return all matches"
```

---

### Task 4: Add signature tables to DB initialization

Add the `signatures` and `file_signatures` table creation to the startup path.

**Files:**
- Modify: `src/ruleengine.cpp` (add table creation in `createTables()`)
- Modify: `include/ruleengine.h` (add `#include "ducksig.h"`)
- Test: `test/test_ruleengine.cpp`

**Step 1: Write the failing test**

```cpp
TEST_CASE("createTables creates signature tables") {
  LlamaDB db;
  LlamaDBConnection conn(db);
  LlamaRuleEngine engine;
  engine.createTables(conn);

  // Verify signature tables exist by inserting into them
  duckdb_result result;
  auto state = duckdb_query(conn.get(),
    "INSERT INTO signatures VALUES ('id-1', 'PDF', 'Portable Document Format')", &result);
  REQUIRE(state != DuckDBError);
  duckdb_destroy_result(&result);

  state = duckdb_query(conn.get(),
    "INSERT INTO file_signatures VALUES ('hash-1', 'id-1')", &result);
  REQUIRE(state != DuckDBError);
  duckdb_destroy_result(&result);
}
```

**Step 2: Run test to verify it fails**

Run: `meson compile -C builddir && builddir/llama_test "createTables creates signature tables"`
Expected: FAIL — INSERT fails because tables don't exist.

**Step 3: Implement**

In `src/ruleengine.cpp`, add to `createTables()`:

```cpp
DBType<SigRec> sigRec;
THROW_IF(!sigRec.createTable(dbConn.get(), "signatures"), "Error creating signatures table");
DBType<FileSigResult> fileSigResult;
THROW_IF(!fileSigResult.createTable(dbConn.get(), "file_signatures"), "Error creating file_signatures table");
```

Add `#include "ducksig.h"` to the includes.

**Step 4: Run test to verify it passes**

Run: `meson compile -C builddir && builddir/llama_test "createTables creates signature tables"`
Expected: PASS

**Step 5: Commit**

```bash
git add src/ruleengine.cpp include/ruleengine.h
git commit -m "Add signatures and file_signatures table creation to startup"
```

---

### Task 5: Add `--signatures` CLI option and load signatures in `Llama::init()`

Add a CLI option for the signatures file path (defaulting to `./magics.json`).
Parse the file and compile the Lightgrep program during `init()`. Populate the
`signatures` reference table in `search()` before processing begins.

**Files:**
- Modify: `include/options.h` (add `SignaturesPath` field)
- Modify: `src/cli.cpp` (add `--signatures` option)
- Modify: `include/llama.h` (add `MagicsType` member)
- Modify: `src/llama.cpp` (add async init future, populate reference table)
- Test: `test/test_llama.cpp`
- Test: `test/test_cli.cpp`
- Reference: `include/filesignatures.h`
- Reference: `include/easyfut.h`

**Step 1: Write the failing test**

In `test/test_cli.cpp`, add a test for the new option (follow existing patterns
in that file). In `test/test_llama.cpp`, test the round-trip: parse magics,
populate DB, verify rows.

```cpp
// test_cli.cpp
TEST_CASE("signatures path defaults to ./magics.json") {
  // Parse with no --signatures flag, verify default
  // (adapt to existing test patterns in test_cli.cpp)
}
```

```cpp
// test_llama.cpp
TEST_CASE("Signature reference table is populated from magics") {
  LlamaDB db;
  LlamaDBConnection conn(db);

  DBType<SigRec>::createTable(conn.get(), "signatures");

  auto magicsResult = FileSignatures::FileSigAnalyzer::readMagics("./magics.json");
  REQUIRE(magicsResult.has_value());
  auto& magics = magicsResult.value();

  // Populate the signatures table
  LlamaDBAppender appender(conn.get(), "signatures");
  SigBatch batch;
  for (const auto& m : magics) {
    batch.add(SigRec{m->Id, m->Name, m->Description});
  }
  batch.copyToDB(appender.get());
  appender.flush();

  duckdb_result result;
  duckdb_query(conn.get(), "SELECT count(*) FROM signatures", &result);
  auto count = duckdb_value_int64(&result, 0, 0);
  duckdb_destroy_result(&result);

  REQUIRE(count == static_cast<int64_t>(magics.size()));
  REQUIRE(count > 0);
}
```

**Step 2: Run test to verify it fails**

Run: `meson compile -C builddir && builddir/llama_test "Signature reference table is populated from magics"`
Expected: Should pass once `ducksig.h` and `readMagics()` changes from prior tasks are in place. If it passes, good — it validates the integration path. If not, fix.

**Step 3: Implement CLI option, init, and search changes**

In `include/options.h`:
- Add field: `std::string SignaturesPath;`

In `src/cli.cpp`:
- Add to `configOpts`:
  ```cpp
  ("signatures",
    po::value<std::string>(&Opts->SignaturesPath)
    ->default_value("./magics.json")
    ->value_name("SIGNATURES_FILE"),
    "Path to file signature definitions (magics.json)")
  ```

In `include/llama.h`:
- Add `#include "filesignatures.h"` and `#include "ducksig.h"`
- Add member: `FileSignatures::MagicsType SigMagics;`
- Add private method: `bool loadSignatures();`

In `src/llama.cpp`:

Add `loadSignatures()`:
```cpp
bool Llama::loadSignatures() {
  auto result = FileSignatures::FileSigAnalyzer::readMagics(Opts->SignaturesPath);
  if (result.has_error()) {
    std::cerr << "Error loading signatures: " << result.error() << std::endl;
    return false;
  }
  SigMagics = std::move(result.value());
  return true;
}
```

In `init()`, add a new future:
```cpp
auto sigs = make_future(Pool, [this]() {
  return loadSignatures();
});
```
Add `sigs.get()` to the return expression.

In `search()`, after `RuleEngine->createTables(DbConn)`, populate the reference table:
```cpp
{
  LlamaDBAppender sigAppender(DbConn.get(), "signatures");
  SigBatch sigBatch;
  for (const auto& m : SigMagics) {
    sigBatch.add(SigRec{m->Id, m->Name, m->Description});
  }
  sigBatch.copyToDB(sigAppender.get());
  sigAppender.flush();
}
```

Pass `SigMagics` into `ProcessorContext` (done in Task 6).

**Step 4: Run test to verify it passes**

Run: `meson compile -C builddir && builddir/llama_test "Signature reference table is populated from magics"`
Expected: PASS

**Step 5: Commit**

```bash
git add include/options.h src/cli.cpp include/llama.h src/llama.cpp test/test_llama.cpp test/test_cli.cpp
git commit -m "Add --signatures CLI option, load signatures in init()"
```

---

### Task 6: Extend `ProcessorContext` and `Processor` to detect and store signatures

Add the `FileSigAnalyzer` to each `Processor` and store detection results.

**Files:**
- Modify: `include/processor.h`
- Modify: `src/processor.cpp`
- Reference: `include/filesignatures.h`
- Reference: `include/ducksig.h`

**Step 1: Write the failing test**

In `test/test_processor.cpp`:

```cpp
#include "ducksig.h"
#include "filesignatures.h"
#include "readseek_impl.h"

TEST_CASE("Processor stores signature detections") {
  // Set up DB
  LlamaDB db;
  LlamaDBConnection conn(db);
  DBType<HashRec>::createTable(conn.get(), "hash");
  DBType<SearchHit>::createTable(conn.get(), "search_hits");
  DBType<RuleRec>::createTable(conn.get(), "rules");
  DBType<RuleMatch>::createTable(conn.get(), "rule_hits");
  DBType<SigRec>::createTable(conn.get(), "signatures");
  DBType<FileSigResult>::createTable(conn.get(), "file_signatures");

  // Load magics and create analyzer
  auto magicsResult = FileSignatures::FileSigAnalyzer::readMagics("./magics.json");
  REQUIRE(magicsResult.has_value());

  auto ruleEngine = std::make_shared<LlamaRuleEngine>();
  auto procContext = std::make_shared<ProcessorContext>(
    &db, nullptr, ruleEngine, "", "", magicsResult.value()
  );
  Processor proc(procContext);

  // Create a PDF-like ReadSeek
  std::vector<uint8_t> pdfBytes = {'%', 'P', 'D', 'F', '-', '1', '.', '4'};
  // ... process an Entry with this data and verify file_signatures has rows

  // Verify file_signatures table has entries after flush
  proc.flush();

  duckdb_result result;
  duckdb_query(conn.get(), "SELECT count(*) FROM file_signatures", &result);
  auto count = duckdb_value_int64(&result, 0, 0);
  duckdb_destroy_result(&result);
  REQUIRE(count > 0);
}
```

Note: The exact test setup will depend on how `Entry` is constructed in existing tests. Check `test/test_processor.cpp` for the existing pattern and adapt. The key assertion is that after processing a file with known magic bytes, the `file_signatures` table has rows.

**Step 2: Run test to verify it fails**

Run: `meson compile -C builddir && builddir/llama_test "Processor stores signature detections"`
Expected: Compilation fails — `ProcessorContext` doesn't accept `MagicsType` yet.

**Step 3: Implement**

In `include/processor.h`:
- Add `#include "ducksig.h"` and `#include "filesignatures.h"`
- Add to `ProcessorContext`:
  ```cpp
  FileSignatures::MagicsType SigMagics;
  ```
- Extend `ProcessorContext` constructor to accept `const FileSignatures::MagicsType& sigMagics`
- Add to `Processor` private members:
  ```cpp
  LlamaDBAppender FileSigAppender;
  std::unique_ptr<FileSigBatch> FileSigs;
  FileSignatures::FileSigAnalyzer SigAnalyzer;
  ```

In `src/processor.cpp`:
- Update `ProcessorContext` constructor to store `sigMagics`
- Update `Processor` constructor to initialize `FileSigAppender`, `FileSigs`, and `SigAnalyzer`
- In `process()`, after hashing and before grep search:
  ```cpp
  // Detect file signatures
  std::vector<FileSignatures::MagicPtr> sigResults;
  entry.getStream().seek(0);
  SigAnalyzer.getSignatures(entry.getStream(), sigResults);
  for (const auto& sig : sigResults) {
    FileSigs->add(FileSigResult{HashRecord.Blake3, sig->Id});
  }
  ```
- In `flush()`, add:
  ```cpp
  FileSigs->copyToDB(FileSigAppender.get());
  FileSigAppender.flush();
  ```

Update `src/llama.cpp` to pass `SigMagics` when constructing `ProcessorContext`.

**Step 4: Run tests to verify they pass**

Run: `meson compile -C builddir && builddir/llama_test "Processor stores signature"` and also run the full suite to ensure no regressions.
Expected: PASS

**Step 5: Commit**

```bash
git add include/processor.h src/processor.cpp src/llama.cpp
git commit -m "Detect and store file signatures in Processor"
```

---

### Task 7: Extend `QueryBuilder` to handle `signature:` rule sections

Generate SQL JOINs for rules with `signature:` sections.

**Files:**
- Modify: `src/querybuilder.cpp`
- Modify: `include/querybuilder.h`
- Test: `test/test_querybuilder.cpp`

**Step 1: Write the failing test**

```cpp
TEST_CASE("buildSqlQueryFromRuleWithSignatureName") {
  std::string input = R"(rule SigRule { signature: name == "PDF" })";
  LlamaParser parser(input, LlamaLexer::getTokens(input, "test"));
  QueryBuilder qb(parser);
  std::vector<Rule> rules = parser.parseRules({0});
  FieldHasher hasher;
  FieldHash hash = rules.at(0).getHash(parser, hasher);
  REQUIRE(rules.at(0).Name == "SigRule");
  auto query = qb.buildSqlQuery(hash, rules.at(0));
  // Should contain JOIN to file_signatures and signatures tables
  REQUIRE(query.find("file_signatures") != std::string::npos);
  REQUIRE(query.find("signatures") != std::string::npos);
  REQUIRE(query.find("Name == 'PDF'") != std::string::npos);
}

TEST_CASE("buildSqlQueryFromRuleWithSignatureId") {
  std::string input = R"(rule SigRule { signature: id == "b9523fad-6835-403f-9be6-91fd678b473f" })";
  LlamaParser parser(input, LlamaLexer::getTokens(input, "test"));
  QueryBuilder qb(parser);
  std::vector<Rule> rules = parser.parseRules({0});
  FieldHasher hasher;
  FieldHash hash = rules.at(0).getHash(parser, hasher);
  auto query = qb.buildSqlQuery(hash, rules.at(0));
  REQUIRE(query.find("Id == 'b9523fad-6835-403f-9be6-91fd678b473f'") != std::string::npos);
}

TEST_CASE("buildSqlQueryFromRuleWithCompoundSignature") {
  std::string input = R"(rule SigRule { signature: name == "PDF" or name == "JPEG" })";
  LlamaParser parser(input, LlamaLexer::getTokens(input, "test"));
  QueryBuilder qb(parser);
  std::vector<Rule> rules = parser.parseRules({0});
  FieldHasher hasher;
  FieldHash hash = rules.at(0).getHash(parser, hasher);
  auto query = qb.buildSqlQuery(hash, rules.at(0));
  REQUIRE(query.find("Name == 'PDF'") != std::string::npos);
  REQUIRE(query.find("OR") != std::string::npos);
  REQUIRE(query.find("Name == 'JPEG'") != std::string::npos);
}
```

**Step 2: Run test to verify it fails**

Run: `meson compile -C builddir && builddir/llama_test "buildSqlQueryFromRuleWithSignatureName"`
Expected: FAIL — query doesn't contain signature JOINs.

**Step 3: Implement**

In `src/querybuilder.cpp`:

Add a signature property lookup map:

```cpp
const static std::unordered_map<std::string_view, std::string> SignaturePropertySqlLookup {
  {"name", "Name"},
  {"id", "Id"}
};
```

The `buildSqlClauseImpl(PropertyNode*)` method currently always uses `FileMetadataPropertySqlLookup`. It needs to know which section's lookup to use. Two approaches:
- (a) Add a section context parameter
- (b) Check both maps

Since `file_metadata` and `signature` property names don't overlap, option (b) is simplest: try `FileMetadataPropertySqlLookup` first, then `SignaturePropertySqlLookup`.

Alternatively (cleaner): add a `buildSignatureClauseImpl` that uses the signature lookup, and call it from `buildSqlQuery` when `rule.Signature` is set. This keeps the two code paths separate.

In `buildSqlQuery()`, when `rule.Signature` is non-null:

```cpp
if (rule.Signature) {
  query += " AND hash.Blake3 IN (SELECT fs.FileHash FROM file_signatures fs"
           " JOIN signatures s ON fs.SigId = s.Id WHERE ";
  buildSignatureClauseImpl(rule.Signature, query);
  query += ")";
}
```

Using a subquery avoids changing the main FROM clause and handles the JOIN cleanly. The `buildSignatureClauseImpl` delegates to `buildSqlClauseImpl` for BoolNodes but uses `SignaturePropertySqlLookup` for PropertyNodes.

Add to `include/querybuilder.h`:
```cpp
void buildSignatureClauseImpl(const Node* n, std::string& out);
void buildSignaturePropertyImpl(const PropertyNode* pn, std::string& out);
```

**Step 4: Run tests to verify they pass**

Run: `meson compile -C builddir && builddir/llama_test "[buildSqlQuery]"` (or run all querybuilder tests)
Expected: PASS for all query builder tests including new signature ones.

**Step 5: Commit**

```bash
git add src/querybuilder.cpp include/querybuilder.h test/test_querybuilder.cpp
git commit -m "Extend QueryBuilder to generate SQL for signature rule sections"
```

---

### Task 8: Run full test suite and fix any regressions

**Step 1: Run full test suite**

Run: `meson compile -C builddir && builddir/llama_test`
Expected: All tests PASS.

**Step 2: Fix any compilation errors or test failures**

Address issues found. Common things to watch for:
- Places that construct `ProcessorContext` without the new `MagicsType` parameter
- Places that construct `FileSigAnalyzer` with the old interface (hardcoded path)
- Missing includes for `ducksig.h`

**Step 3: Commit fixes if any**

```bash
git add -u
git commit -m "Fix regressions from file signature analysis integration"
```

---

### Task 9: Update build system files

Both Meson and Autotools need to know about `ducksig.h`. Since it's a header-only file (no `.cpp`), Meson and Autotools generally don't need explicit listing for headers. However, verify that any new source files are listed.

If `ducksig.h` is header-only and no new `.cpp` files were created, this task is just verification.

**Step 1: Verify builds**

Run: `meson compile -C builddir && make -j8 check`
Expected: Both build systems compile and tests pass.

**Step 2: Commit if any build file changes needed**

```bash
git add Makefile.am src/meson.build test/meson.build
git commit -m "Update build system for file signature analysis"
```

---

### Task 10: Share compiled program across Processors and cache to disk

Two problems to fix: (a) each `Processor` recompiles the Lightgrep signature
program from scratch, and (b) `LightGrep::search()` creates and destroys an
`LG_HCONTEXT` on every call. Additionally, the compiled program should be
cached to disk so subsequent runs skip compilation entirely.

**Design:**
- An `LG_HPROGRAM` is immutable and can be shared across threads.
- An `LG_HCONTEXT` is NOT threadsafe and must be per-Processor.
- This mirrors the existing grep pattern: shared `LgProg`, per-Processor `LgCtx`.

**Architecture changes:**
1. `Llama::loadSignatures()` compiles the program once (or loads from cache),
   stores it as a shared `LG_HPROGRAM` on `Llama`.
2. `ProcessorContext` stores the shared `LG_HPROGRAM` (replaces `MagicsType`
   for program compilation; still needs `MagicsType` for index-to-signature mapping).
3. Each `Processor`'s `FileSigAnalyzer` creates its own `LG_HCONTEXT` from the
   shared program, rather than recompiling.
4. `LightGrep::search()` uses a persistent `LG_HCONTEXT` member instead of
   creating/destroying one per call.

**Disk caching:**
- Cache location: `~/.llama/cache/magics.lgp`
- `lg_write_program()` and `lg_read_program()` work with **buffers**, not file
  paths. Use `lg_program_size()` to allocate, then read/write the buffer to disk.
- Cache invalidation: compare `magics.json` mtime against cache file mtime.
- Cache write failure is non-fatal (program still works, just slower next time).

**Files:**
- Modify: `include/filesignatures.h`
- Modify: `src/filesignatures.cpp`
- Modify: `include/processor.h`
- Modify: `src/processor.cpp`
- Modify: `include/llama.h`
- Modify: `src/llama.cpp`
- Test: `test/test_filesignatures.cpp`
- Reference: `../lightgrep/include/lightgrep/api.h` — `lg_write_program()`,
  `lg_read_program()`, `lg_program_size()`

**Sub-task 10a: Refactor LightGrep to hold a persistent LG_HCONTEXT**

Currently `LightGrep::search()` creates and destroys an `LG_HCONTEXT` per call.
Change it to hold a persistent context as a member, created once from the program.

Step 1: Write failing test — test that `LightGrep` can be constructed with
an external `LG_HPROGRAM` and search correctly.

Step 2: Implement:
- Add a constructor `LightGrep(LG_HPROGRAM prog)` that takes a shared program
  (does not own it — no destroy in destructor for this case).
- Add `LG_HCONTEXT Ctx` member, created in a new `createContext()` method.
- Change `search()` to use `Ctx` instead of creating/destroying per call.
  Call `lg_reset_context()` before each search.
- Existing `setup()` calls `createContext()` after compiling.

Step 3: Run tests to verify.

**Sub-task 10b: Refactor FileSigAnalyzer to accept a shared program**

Add a constructor that takes a pre-compiled `LG_HPROGRAM` + `MagicsType`,
skipping compilation. The existing constructor still compiles for standalone use.

Step 1: Write failing test — construct `FileSigAnalyzer` with a shared program,
verify it detects signatures.

Step 2: Implement:
- Add constructor `FileSigAnalyzer(LG_HPROGRAM sharedProg, const MagicsType& magics)`
- This constructor sorts magics (same as existing), creates `LightGrep` from
  the shared program, sizes `ReadBuf`.
- `lgCallbackfn` and `lgSearch` work unchanged.

Step 3: Run tests to verify.

**Sub-task 10c: Wire shared program through ProcessorContext**

Step 1: Write failing test — construct `ProcessorContext` with a shared program,
verify `Processor` can detect signatures.

Step 2: Implement:
- Add `LG_HPROGRAM SigProg` to `ProcessorContext` (the shared compiled program).
- Keep `MagicsType SigMagics` on `ProcessorContext` (needed for index mapping).
- `Processor` constructs `FileSigAnalyzer(Context->SigProg, Context->SigMagics)`
  instead of `FileSigAnalyzer(Context->SigMagics)`.
- `Llama::loadSignatures()` compiles the program and stores it on `Llama`.
- `Llama::search()` passes the shared program to `ProcessorContext`.

Step 3: Run full test suite.

**Sub-task 10d: Add program serialization and disk caching**

Step 1: Write failing test:

```cpp
TEST_CASE("LightGrep program round-trips through serialization") {
  // readMagics, setup LightGrep, get program
  // writeProgram to a temp file
  // readProgram from that file
  // Construct a new FileSigAnalyzer from the deserialized program
  // Verify it detects PDF signatures
  // Clean up temp file
}
```

Step 2: Implement:
- Add to `LightGrep`:
  ```cpp
  expected<bool> writeProgram(const std::string& path) const;
  static expected<LG_HPROGRAM> readProgram(const std::string& path);
  ```
- `writeProgram()`: call `lg_program_size(Prog)` to get size, allocate buffer,
  call `lg_write_program(Prog, buffer)`, write buffer to file.
- `readProgram()`: read file into buffer, call `lg_read_program(buffer, size)`.
- Update `Llama::loadSignatures()`:
  - Always parse `magics.json` (needed for index mapping).
  - Cache path: `~/.llama/cache/magics.lgp`. Create `~/.llama/cache/` if needed.
  - If cache exists and is newer than `magics.json`, load from cache.
  - Otherwise compile and write cache (failure is non-fatal).

Step 3: Run tests to verify.

Step 4: Run full test suite.

**Step 5: Commit**

Commit after each sub-task passes, or as a single commit if done atomically:

```bash
git commit -m "Share compiled signature program across Processors and cache to disk"
```
