# Binary Hash Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Flip every cryptographic-hash column and every hash-derived 32-byte ID column in llama from `VARCHAR`-hex to `BLOB`-bytes, change the plugin FFI `sha256` field to inline `uint8_t[32]`, and extend `llamaduck.h`'s reflection to handle `std::array<uint8_t, N>` and `std::optional<std::array<uint8_t, N>>` columns.

**Architecture:** Foundation-first — drop in the `BitsetVar` utility and `llamaduck.h` reflection extensions (with their own focused tests) before touching production structs. Then flip data-structure headers in dependency order, each landing build-green with existing tests still passing. Finally reshape `HashRec` together with all its `processor.cpp` and plugin-FFI consumers in a single atomic task.

**Tech Stack:** C++20 (`boost::pfr` reflection), DuckDB C API, Meson build system, Catch2-style test runner.

**Source-of-truth spec:** `docs/superpowers/specs/2026-05-25-binary-hash-migration-design.md`. Where this plan references spec line ranges, the spec is authoritative.

**Build/test commands** (per repo conventions):
- Compile: `meson compile -C build`
- Test: `meson test -C build --verbose`
- Single test by name: `meson test -C build --verbose <test-suite-name>`

---

## File Structure

### Files to create
- `include/bitsetvar.h` — growable single-bit-per-element bitset utility
- `test/test_bitsetvar.cpp` — `BitsetVar` unit tests
- `test/test_dbbatch_binary.cpp` — fixture-driven `DBBatch<T>` BLOB round-trip tests covering `appendRecord` interleavings

### Files to modify (rough dependency order)
- `include/fieldhash.h` — `uint8_t hash[32]` → `std::array<uint8_t, 32> hash`
- `include/llamaduck.h` — type traits, constexpr column counters, `DBBatch` storage members, `appendRecord` rewrite
- `src/direntbatch.cpp` — new `appendVal(uint8_t*, size_t)` BLOB overload
- `include/inode.h`, `include/entry.h`, `include/extent.h`, `include/direntbatch.h`, `include/evidencerec.h`, `include/llamabatch.h`, `include/ducksig.h` — `std::string Id` (or `uint8_t[32]`) fields flip to `std::array<uint8_t, 32>`
- `include/duckhash.h` — full `HashRec` reshape (member types + `set()` + helpers)
- `include/plugin_api.h` — `const char* sha256` → `uint8_t sha256[32]`
- `include/processor.h` — `setSHA256` signature
- `src/recordhasher.cpp`, `src/tskreader.cpp`, `src/direntstack.cpp`, `src/tskimgassembler.cpp`, `src/posixreader.cpp`, `src/processor.cpp` — consumer-side assignment / emptiness / memcpy updates
- `src/parser.cpp` — verify and (if needed) decode hash literals to bytes at parse time
- `docs/core/database-schema.md` — column types VARCHAR → BLOB; `unhex` / `lower(hex())` idiom notes

---

## Task 1: `BitsetVar` utility + unit tests

**Files:**
- Create: `include/bitsetvar.h`
- Create: `test/test_bitsetvar.cpp`
- Modify: `meson.build` (register `test_bitsetvar.cpp` if test files are enumerated)

- [ ] **Step 1: Write the failing test file**

Create `test/test_bitsetvar.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "bitsetvar.h"

TEST_CASE("BitsetVar default-constructs empty", "[bitsetvar]") {
  BitsetVar b;
  REQUIRE(b.size() == 0);
}

TEST_CASE("BitsetVar push_back grows correctly across byte boundaries", "[bitsetvar]") {
  BitsetVar b;
  for (size_t target : {size_t{1}, size_t{7}, size_t{8}, size_t{9}, size_t{17}}) {
    BitsetVar fresh;
    for (size_t i = 0; i < target; ++i) {
      fresh.push_back(i % 2 == 0);
    }
    REQUIRE(fresh.size() == target);
    for (size_t i = 0; i < target; ++i) {
      REQUIRE(fresh.get(i) == (i % 2 == 0));
    }
  }
}

TEST_CASE("BitsetVar set flips bits on and off", "[bitsetvar]") {
  BitsetVar b;
  for (size_t i = 0; i < 20; ++i) b.push_back(false);
  REQUIRE_FALSE(b.get(13));
  b.set(13, true);
  REQUIRE(b.get(13));
  b.set(13, false);
  REQUIRE_FALSE(b.get(13));
}

TEST_CASE("BitsetVar clear resets size and storage", "[bitsetvar]") {
  BitsetVar b;
  for (size_t i = 0; i < 20; ++i) b.push_back(true);
  b.clear();
  REQUIRE(b.size() == 0);
  b.push_back(false);
  REQUIRE(b.size() == 1);
  REQUIRE_FALSE(b.get(0));
}

TEST_CASE("BitsetVar reserve allocates without changing observable state", "[bitsetvar]") {
  BitsetVar b;
  b.reserve(100);
  REQUIRE(b.size() == 0);
  b.push_back(true);
  REQUIRE(b.get(0));
}
```

- [ ] **Step 2: Run test to verify it fails to compile**

Run: `meson compile -C build`
Expected: FAIL with "bitsetvar.h: No such file or directory"

- [ ] **Step 3: Create `include/bitsetvar.h`**

```cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

class BitsetVar {
public:
  void push_back(bool value) {
    if (Size % 8 == 0) Bits.push_back(0);
    if (value) Bits.back() |= uint8_t(1u << (Size % 8));
    ++Size;
  }

  bool get(size_t index) const {
    return Bits[index / 8] & uint8_t(1u << (index % 8));
  }

  void set(size_t index, bool value) {
    const auto mask = uint8_t(1u << (index % 8));
    if (value) Bits[index / 8] |= mask;
    else       Bits[index / 8] &= uint8_t(~mask);
  }

  size_t size() const { return Size; }
  void clear() { Bits.clear(); Size = 0; }
  void reserve(size_t n) { Bits.reserve((n + 7) / 8); }

private:
  std::vector<uint8_t> Bits;
  size_t Size = 0;
};
```

- [ ] **Step 4: Register the new test file in meson**

Open `meson.build` (or `test/meson.build`) and locate the existing test source list. Add `test_bitsetvar.cpp` to the list, following the same pattern as adjacent entries (e.g. `test_hex.cpp`).

- [ ] **Step 5: Run tests to verify they pass**

Run: `meson compile -C build`
Expected: build green

Run: `meson test -C build --verbose`
Expected: all tests pass, including the new `bitsetvar` cases

- [ ] **Step 6: Commit**

```bash
git add include/bitsetvar.h test/test_bitsetvar.cpp meson.build
git commit -m "feat(bitsetvar): add growable bit-per-element bitset utility"
```

---

## Task 2: `llamaduck.h` row-major `appendRecord` rewrite (existing types only)

Restructure `DBBatch<T>::appendRecord` and `copyToDB` to use row-major addressing through compile-time helpers. Today's backward-walking `index` parameter goes away. Binary support is added in Task 3 — this task ONLY rewrites how existing integer and string columns are addressed, and adds stub traits so Task 3 plugs in cleanly. Existing tests must remain green.

**Files:**
- Modify: `include/llamaduck.h` (lines 176-247, the `DBBatch<T>` struct)
- Test: `test/test_duckdb.cpp` (existing — must remain green)

- [ ] **Step 1: Add stub traits for binary detection**

Open `include/llamaduck.h`. Locate `duckdbType<T>()` (the existing reflection-driven type-string emitter). Above the `DBBatch<T>` struct, add:

```cpp
#include <array>
#include <optional>

template<typename T> struct is_byte_array : std::false_type {};
template<size_t N>   struct is_byte_array<std::array<uint8_t, N>> : std::true_type {};
template<typename T> inline constexpr bool is_byte_array_v = is_byte_array<T>::value;

template<typename T> struct is_optional_byte_array : std::false_type {};
template<size_t N>   struct is_optional_byte_array<std::optional<std::array<uint8_t, N>>> : std::true_type {};
template<typename T> inline constexpr bool is_optional_byte_array_v = is_optional_byte_array<T>::value;
```

These specialize for binary types but won't match any production struct yet (no struct uses `std::array<uint8_t, N>` until Task 5+).

- [ ] **Step 2: Add constexpr column-ordinal helpers**

Below the traits, add:

```cpp
template<typename TupleType, size_t I = 0>
constexpr size_t numNonBinaryCols() {
  if constexpr (I >= std::tuple_size_v<TupleType>) return 0;
  else {
    using C = std::tuple_element_t<I, TupleType>;
    if constexpr (is_byte_array_v<C> || is_optional_byte_array_v<C>)
      return numNonBinaryCols<TupleType, I + 1>();
    else
      return 1 + numNonBinaryCols<TupleType, I + 1>();
  }
}

template<typename TupleType, size_t CurIndex, size_t I = 0>
constexpr size_t nonBinaryColIndex() {
  static_assert(CurIndex <= std::tuple_size_v<TupleType>);
  if constexpr (I >= CurIndex) return 0;
  else {
    using C = std::tuple_element_t<I, TupleType>;
    if constexpr (is_byte_array_v<C> || is_optional_byte_array_v<C>)
      return nonBinaryColIndex<TupleType, CurIndex, I + 1>();
    else
      return 1 + nonBinaryColIndex<TupleType, CurIndex, I + 1>();
  }
}
```

For tuples with no binary columns (every production struct today), `numNonBinaryCols == NumCols` and `nonBinaryColIndex<...,CurIndex>() == CurIndex` — identity behavior.

- [ ] **Step 3: Rewrite `appendRecord` and `copyToDB` row-major**

Replace lines 221-246 in `include/llamaduck.h`:

```cpp
  template<size_t CurIndex>
  void appendRecord(duckdb_appender& appender, size_t row) {
    using TupleType = typename DBType<T>::TupleType;
    using ColumnType = typename std::tuple_element<CurIndex, TupleType>::type;

    if constexpr (CurIndex > 0) {
      appendRecord<CurIndex - 1>(appender, row);
    }

    if constexpr (std::is_integral_v<ColumnType>) {
      const size_t slot = row * numNonBinaryCols<TupleType>() + nonBinaryColIndex<TupleType, CurIndex>();
      appendVal(appender, OffsetVals[slot]);
    }
    else if constexpr (std::is_convertible_v<ColumnType, std::string>) {
      const size_t slot = row * numNonBinaryCols<TupleType>() + nonBinaryColIndex<TupleType, CurIndex>();
      appendVal(appender, Buf.data() + OffsetVals[slot]);
    }
  }

  size_t copyToDB(duckdb_appender& appender) {
    for (size_t row = 0; row < NumRows; ++row) {
      duckdb_appender_begin_row(appender);
      appendRecord<DBType<T>::NumCols - 1>(appender, row);
      duckdb_appender_end_row(appender);
    }
    return NumRows;
  }
```

- [ ] **Step 4: Build and run all existing tests**

Run: `meson compile -C build`
Expected: build green

Run: `meson test -C build --verbose`
Expected: every existing test passes — particularly `test_duckdb` (the existing `DBBatch` integration tests at `test/test_duckdb.cpp:118,389,416,460`).

If any test fails, the row-major slot math is wrong. Inspect the `slot` computation against `OffsetVals` ordering: `add()` writes `OffsetVals` in tuple order (column 0 first, then column 1, ...), and within one row that ordering must match what `appendRecord` reads.

- [ ] **Step 5: Commit**

```bash
git add include/llamaduck.h
git commit -m "refactor(llamaduck): row-major DBBatch::appendRecord addressing"
```

---

## Task 3: `llamaduck.h` binary column support

Add the `BinaryBuf` + `BinaryNull` storage, the binary-column counters and offsets, the `add()` and `appendRecord` binary branches, the `duckdbType` BLOB branch, and the `appendVal(uint8_t*, size_t)` overload. Cover with the fixture-driven interleaving tests (which exercise every shape called out in the spec testing strategy).

**Files:**
- Modify: `include/llamaduck.h`
- Modify: `src/direntbatch.cpp` (new `appendVal` overload — co-located with existing overloads at lines 4, 9)
- Create: `test/test_dbbatch_binary.cpp`
- Modify: `meson.build` (register new test file)

- [ ] **Step 1: Add binary-column constexpr helpers in llamaduck.h**

Below the `numNonBinaryCols` / `nonBinaryColIndex` helpers added in Task 2, add:

```cpp
template<typename T> struct byte_array_size : std::integral_constant<size_t, 0> {};
template<size_t N>   struct byte_array_size<std::array<uint8_t, N>> : std::integral_constant<size_t, N> {};
template<typename T> inline constexpr size_t byte_array_size_v = byte_array_size<T>::value;

template<typename TupleType, size_t I = 0>
constexpr size_t numBinaryCols() {
  if constexpr (I >= std::tuple_size_v<TupleType>) return 0;
  else {
    using C = std::tuple_element_t<I, TupleType>;
    if constexpr (is_byte_array_v<C> || is_optional_byte_array_v<C>)
      return 1 + numBinaryCols<TupleType, I + 1>();
    else
      return numBinaryCols<TupleType, I + 1>();
  }
}

template<typename TupleType, size_t CurIndex, size_t I = 0>
constexpr size_t binaryColIndex() {
  static_assert(CurIndex <= std::tuple_size_v<TupleType>);
  if constexpr (I >= CurIndex) return 0;
  else {
    using C = std::tuple_element_t<I, TupleType>;
    if constexpr (is_byte_array_v<C> || is_optional_byte_array_v<C>)
      return 1 + binaryColIndex<TupleType, CurIndex, I + 1>();
    else
      return binaryColIndex<TupleType, CurIndex, I + 1>();
  }
}

template<typename TupleType, size_t I = 0>
constexpr size_t numNullableBinaryCols() {
  if constexpr (I >= std::tuple_size_v<TupleType>) return 0;
  else {
    using C = std::tuple_element_t<I, TupleType>;
    if constexpr (is_optional_byte_array_v<C>)
      return 1 + numNullableBinaryCols<TupleType, I + 1>();
    else
      return numNullableBinaryCols<TupleType, I + 1>();
  }
}

template<typename TupleType, size_t CurIndex, size_t I = 0>
constexpr size_t nullableBinaryColIndex() {
  static_assert(CurIndex <= std::tuple_size_v<TupleType>);
  if constexpr (I >= CurIndex) return 0;
  else {
    using C = std::tuple_element_t<I, TupleType>;
    if constexpr (is_optional_byte_array_v<C>)
      return 1 + nullableBinaryColIndex<TupleType, CurIndex, I + 1>();
    else
      return nullableBinaryColIndex<TupleType, CurIndex, I + 1>();
  }
}

template<typename TupleType, size_t I = 0>
constexpr size_t rowBinaryBytes() {
  if constexpr (I >= std::tuple_size_v<TupleType>) return 0;
  else {
    using C = std::tuple_element_t<I, TupleType>;
    if constexpr (is_byte_array_v<C>)
      return byte_array_size_v<C> + rowBinaryBytes<TupleType, I + 1>();
    else if constexpr (is_optional_byte_array_v<C>)
      return byte_array_size_v<typename C::value_type> + rowBinaryBytes<TupleType, I + 1>();
    else
      return rowBinaryBytes<TupleType, I + 1>();
  }
}

// J is a binary-column ordinal (0 = first binary col in tuple order).
template<typename TupleType, size_t J, size_t I = 0, size_t Seen = 0>
constexpr size_t binaryColOffset() {
  if constexpr (I >= std::tuple_size_v<TupleType>) return 0;
  else {
    using C = std::tuple_element_t<I, TupleType>;
    if constexpr (is_byte_array_v<C>) {
      if constexpr (Seen == J) return 0;
      else return byte_array_size_v<C> + binaryColOffset<TupleType, J, I + 1, Seen + 1>();
    }
    else if constexpr (is_optional_byte_array_v<C>) {
      if constexpr (Seen == J) return 0;
      else return byte_array_size_v<typename C::value_type> + binaryColOffset<TupleType, J, I + 1, Seen + 1>();
    }
    else {
      return binaryColOffset<TupleType, J, I + 1, Seen>();
    }
  }
}
```

- [ ] **Step 2: Extend `duckdbType<T>()` for BLOB**

In the `duckdbType<T>()` function (already in `llamaduck.h`), add a branch:

```cpp
else if constexpr (is_byte_array_v<T> || is_optional_byte_array_v<T>) {
    return "BLOB";
}
```

Place it alongside the existing integer / string branches. Follow the existing convention — no `NOT NULL` suffix in the type string.

- [ ] **Step 3: Add `BinaryBuf` and `BinaryNull` members to `DBBatch<T>`**

In the `DBBatch<T>` struct (around llamaduck.h:177), add storage members alongside the existing `Buf` and `OffsetVals`:

```cpp
  std::vector<uint8_t> BinaryBuf;   // R * rowBinaryBytes<TupleType>(), zero-filled for nulls
  BitsetVar            BinaryNull;  // R * numNullableBinaryCols<TupleType>() bits
```

Add the `#include "bitsetvar.h"` at the top of `llamaduck.h`.

Update `clear()` to also clear the new members:

```cpp
  void clear() {
    Buf.clear();
    OffsetVals.clear();
    BinaryBuf.clear();
    BinaryNull.clear();
    NumRows = 0;
  }
```

- [ ] **Step 4: Add the BLOB `appendVal` overload**

In `src/direntbatch.cpp` (which already owns the `const char*` and `uint64_t` overloads at lines 4 and 9), add a third overload:

```cpp
void appendVal(duckdb_appender& appender, const uint8_t* data, size_t len) {
    duckdb_state state = duckdb_append_blob(appender, data, len);
    THROW_IF(state == DuckDBError, "duckdb_append_blob failed");
}
```

In `include/llamaduck.h` (around line 107, the existing `appendVal` declarations), add:

```cpp
void appendVal(duckdb_appender& appender, const uint8_t* data, size_t len);
```

- [ ] **Step 5: Extend `add()` with binary branches**

Locate `add()` in `DBBatch<T>` (around llamaduck.h:198). After the existing `is_integral_v` branch, add:

```cpp
    else if constexpr (is_byte_array_v<Cur>) {
      BinaryBuf.insert(BinaryBuf.end(), cur.begin(), cur.end());
      // No BinaryNull push: non-optional columns are NOT NULL by type.
    }
    else if constexpr (is_optional_byte_array_v<Cur>) {
      constexpr size_t N = byte_array_size_v<typename Cur::value_type>;
      if (cur.has_value()) {
        BinaryBuf.insert(BinaryBuf.end(), cur->begin(), cur->end());
        BinaryNull.push_back(false);
      } else {
        BinaryBuf.insert(BinaryBuf.end(), N, uint8_t(0));
        BinaryNull.push_back(true);
      }
    }
```

- [ ] **Step 6: Extend `appendRecord` with binary branches**

In the `appendRecord<CurIndex>` rewritten in Task 2, add two new `if constexpr` branches alongside the integer/string branches:

```cpp
    if constexpr (is_byte_array_v<ColumnType>) {
      constexpr size_t N = byte_array_size_v<ColumnType>;
      using TupleType = typename DBType<T>::TupleType;
      constexpr size_t colOffset = binaryColOffset<TupleType, binaryColIndex<TupleType, CurIndex>()>();
      appendVal(appender,
                BinaryBuf.data() + row * rowBinaryBytes<TupleType>() + colOffset,
                N);
    }
    else if constexpr (is_optional_byte_array_v<ColumnType>) {
      constexpr size_t N = byte_array_size_v<typename ColumnType::value_type>;
      using TupleType = typename DBType<T>::TupleType;
      constexpr size_t colOffset = binaryColOffset<TupleType, binaryColIndex<TupleType, CurIndex>()>();
      const size_t bit = row * numNullableBinaryCols<TupleType>() + nullableBinaryColIndex<TupleType, CurIndex>();
      if (BinaryNull.get(bit)) {
        duckdb_append_null(appender);
      } else {
        appendVal(appender,
                  BinaryBuf.data() + row * rowBinaryBytes<TupleType>() + colOffset,
                  N);
      }
    }
```

(Place these BEFORE the `is_integral_v` and `is_convertible_v<…, std::string>` branches so that `std::array<uint8_t, N>` doesn't accidentally satisfy any later test.)

- [ ] **Step 7: Build, run existing tests**

Run: `meson compile -C build`
Expected: build green

Run: `meson test -C build --verbose`
Expected: all existing tests pass. No production struct uses binary types yet, so the new branches don't fire; this is a regression check on Task 2's row-major rewrite plus the new helpers compiling correctly.

- [ ] **Step 8: Write the fixture-driven BLOB round-trip tests**

Create `test/test_dbbatch_binary.cpp`. The intent (per spec testing strategy) is to exercise every interleaving shape independently of production structs.

```cpp
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <optional>
#include <duckdb.h>
#include "llamaduck.h"

namespace {

struct BinFirst {
  static constexpr auto ColNames = {"id", "name"};
  std::array<uint8_t, 32> id;
  std::string name;
};

struct BinLast {
  static constexpr auto ColNames = {"name", "id"};
  std::string name;
  std::array<uint8_t, 32> id;
};

struct BinEnds {
  static constexpr auto ColNames = {"id", "name", "value", "tag"};
  std::array<uint8_t, 16> id;
  std::string name;
  uint64_t value;
  std::array<uint8_t, 32> tag;
};

struct Alternating {
  static constexpr auto ColNames = {"a", "x", "b", "y"};
  std::array<uint8_t, 8> a;
  uint64_t x;
  std::array<uint8_t, 8> b;
  uint64_t y;
};

struct AllBinary {
  static constexpr auto ColNames = {"a", "b"};
  std::array<uint8_t, 8> a;
  std::array<uint8_t, 16> b;
};

struct OptAndReq {
  static constexpr auto ColNames = {"req", "opt"};
  std::array<uint8_t, 32> req;
  std::optional<std::array<uint8_t, 16>> opt;
};

struct MultiOpt {
  static constexpr auto ColNames = {"id", "opt_a", "opt_b"};
  std::array<uint8_t, 32> id;
  std::optional<std::array<uint8_t, 16>> opt_a;
  std::optional<std::array<uint8_t, 32>> opt_b;
};

template<typename Rec>
struct DuckScope {
  duckdb_database  db{};
  duckdb_connection conn{};
  duckdb_appender   app{};

  DuckScope(const char* table) {
    REQUIRE(duckdb_open(nullptr, &db) == DuckDBSuccess);
    REQUIRE(duckdb_connect(db, &conn) == DuckDBSuccess);
    REQUIRE(DBType<Rec>::createTable(conn, table));
    REQUIRE(duckdb_appender_create(conn, nullptr, table, &app) == DuckDBSuccess);
  }
  ~DuckScope() {
    duckdb_appender_destroy(&app);
    duckdb_disconnect(&conn);
    duckdb_close(&db);
  }
};

}  // namespace

TEST_CASE("DBBatch round-trips binary-first record", "[dbbatch][binary]") {
  DuckScope<BinFirst> scope("t");
  DBBatch<BinFirst> batch;
  BinFirst row{};
  for (size_t i = 0; i < 32; ++i) row.id[i] = uint8_t(i);
  row.name = "hello";
  batch.add(row);
  REQUIRE(batch.copyToDB(scope.app) == 1);
  REQUIRE(duckdb_appender_flush(scope.app) == DuckDBSuccess);
  duckdb_result r;
  REQUIRE(duckdb_query(scope.conn,
    "SELECT typeof(id), length(id), typeof(name) FROM t", &r) == DuckDBSuccess);
  REQUIRE(std::string(duckdb_value_varchar_internal(&r, 0, 0)) == "BLOB");
  REQUIRE(duckdb_value_int64(&r, 1, 0) == 32);
  REQUIRE(std::string(duckdb_value_varchar_internal(&r, 2, 0)) == "VARCHAR");
  duckdb_destroy_result(&r);
}

TEST_CASE("DBBatch round-trips binary-last record", "[dbbatch][binary]") {
  DuckScope<BinLast> scope("t");
  DBBatch<BinLast> batch;
  BinLast row{};
  row.name = "x";
  for (size_t i = 0; i < 32; ++i) row.id[i] = uint8_t(0xA0 | (i & 0x0F));
  batch.add(row);
  REQUIRE(batch.copyToDB(scope.app) == 1);
  REQUIRE(duckdb_appender_flush(scope.app) == DuckDBSuccess);
  duckdb_result r;
  REQUIRE(duckdb_query(scope.conn,
    "SELECT length(id), hex(id) FROM t", &r) == DuckDBSuccess);
  REQUIRE(duckdb_value_int64(&r, 0, 0) == 32);
  duckdb_destroy_result(&r);
}

TEST_CASE("DBBatch round-trips binary-at-both-ends with middle non-binary", "[dbbatch][binary]") {
  DuckScope<BinEnds> scope("t");
  DBBatch<BinEnds> batch;
  for (int i = 0; i < 3; ++i) {
    BinEnds row{};
    for (size_t j = 0; j < 16; ++j) row.id[j] = uint8_t(i * 16 + j);
    row.name = "row" + std::to_string(i);
    row.value = uint64_t(i * 100);
    for (size_t j = 0; j < 32; ++j) row.tag[j] = uint8_t(0xC0 | (j & 0x0F));
    batch.add(row);
  }
  REQUIRE(batch.copyToDB(scope.app) == 3);
  REQUIRE(duckdb_appender_flush(scope.app) == DuckDBSuccess);
  duckdb_result r;
  REQUIRE(duckdb_query(scope.conn,
    "SELECT count(*), sum(value), length(id), length(tag) FROM t", &r) == DuckDBSuccess);
  REQUIRE(duckdb_value_int64(&r, 0, 0) == 3);
  REQUIRE(duckdb_value_int64(&r, 1, 0) == 300);
  duckdb_destroy_result(&r);
}

TEST_CASE("DBBatch round-trips alternating binary/non-binary", "[dbbatch][binary]") {
  DuckScope<Alternating> scope("t");
  DBBatch<Alternating> batch;
  Alternating row{};
  for (size_t j = 0; j < 8; ++j) { row.a[j] = uint8_t(j); row.b[j] = uint8_t(j + 100); }
  row.x = 42; row.y = 999;
  batch.add(row);
  REQUIRE(batch.copyToDB(scope.app) == 1);
  REQUIRE(duckdb_appender_flush(scope.app) == DuckDBSuccess);
  duckdb_result r;
  REQUIRE(duckdb_query(scope.conn,
    "SELECT length(a), x, length(b), y FROM t", &r) == DuckDBSuccess);
  REQUIRE(duckdb_value_int64(&r, 1, 0) == 42);
  REQUIRE(duckdb_value_int64(&r, 3, 0) == 999);
  duckdb_destroy_result(&r);
}

TEST_CASE("DBBatch round-trips all-binary record", "[dbbatch][binary]") {
  DuckScope<AllBinary> scope("t");
  DBBatch<AllBinary> batch;
  AllBinary row{};
  for (size_t j = 0; j < 8; ++j) row.a[j] = uint8_t(j);
  for (size_t j = 0; j < 16; ++j) row.b[j] = uint8_t(j);
  batch.add(row);
  REQUIRE(batch.copyToDB(scope.app) == 1);
  REQUIRE(duckdb_appender_flush(scope.app) == DuckDBSuccess);
  duckdb_result r;
  REQUIRE(duckdb_query(scope.conn,
    "SELECT length(a), length(b) FROM t", &r) == DuckDBSuccess);
  REQUIRE(duckdb_value_int64(&r, 0, 0) == 8);
  REQUIRE(duckdb_value_int64(&r, 1, 0) == 16);
  duckdb_destroy_result(&r);
}

TEST_CASE("DBBatch round-trips optional alongside required binary", "[dbbatch][binary]") {
  DuckScope<OptAndReq> scope("t");
  DBBatch<OptAndReq> batch;
  OptAndReq present{};
  for (size_t j = 0; j < 32; ++j) present.req[j] = uint8_t(0x11);
  std::array<uint8_t, 16> v{};
  for (size_t j = 0; j < 16; ++j) v[j] = uint8_t(0x22);
  present.opt = v;
  batch.add(present);
  OptAndReq absent{};
  for (size_t j = 0; j < 32; ++j) absent.req[j] = uint8_t(0x33);
  absent.opt = std::nullopt;
  batch.add(absent);
  REQUIRE(batch.copyToDB(scope.app) == 2);
  REQUIRE(duckdb_appender_flush(scope.app) == DuckDBSuccess);
  duckdb_result r;
  REQUIRE(duckdb_query(scope.conn,
    "SELECT length(req), opt IS NULL FROM t ORDER BY req", &r) == DuckDBSuccess);
  REQUIRE(duckdb_value_int64(&r, 0, 0) == 32);  // req[0] = 0x11
  REQUIRE(duckdb_value_int64(&r, 1, 0) == 0);   // opt present
  REQUIRE(duckdb_value_int64(&r, 1, 1) == 1);   // opt null
  duckdb_destroy_result(&r);
}

TEST_CASE("DBBatch handles multiple optional binary columns with mixed presence", "[dbbatch][binary]") {
  DuckScope<MultiOpt> scope("t");
  DBBatch<MultiOpt> batch;
  for (int i = 0; i < 4; ++i) {
    MultiOpt row{};
    for (size_t j = 0; j < 32; ++j) row.id[j] = uint8_t(i);
    if (i & 1) {
      std::array<uint8_t, 16> a{};
      for (auto& b : a) b = uint8_t(0xAA);
      row.opt_a = a;
    }
    if (i & 2) {
      std::array<uint8_t, 32> b{};
      for (auto& by : b) by = uint8_t(0xBB);
      row.opt_b = b;
    }
    batch.add(row);
  }
  REQUIRE(batch.copyToDB(scope.app) == 4);
  REQUIRE(duckdb_appender_flush(scope.app) == DuckDBSuccess);
  duckdb_result r;
  REQUIRE(duckdb_query(scope.conn,
    "SELECT sum((opt_a IS NULL)::INT), sum((opt_b IS NULL)::INT) FROM t", &r) == DuckDBSuccess);
  REQUIRE(duckdb_value_int64(&r, 0, 0) == 2);  // rows 0, 2
  REQUIRE(duckdb_value_int64(&r, 1, 0) == 2);  // rows 0, 1
  duckdb_destroy_result(&r);
}
```

(Note on the DuckDB C-API helpers: the snippet above uses idiomatic calls that match the patterns in `test/test_duckdb.cpp`. If a particular helper differs in this codebase's version of the DuckDB header, follow whichever pattern `test_duckdb.cpp` uses.)

Register `test_dbbatch_binary.cpp` in `meson.build` next to `test_dbbatch_*` or `test_duckdb.cpp`.

- [ ] **Step 9: Build and run all tests**

Run: `meson compile -C build`
Expected: build green.

Run: `meson test -C build --verbose`
Expected: all existing tests pass; all new `[dbbatch][binary]` tests pass.

If any binary-test fails:
- For an offset/index mismatch on present cells, recheck `binaryColOffset` recursion: cumulative byte position must match where `add()` actually wrote the bytes.
- For an `IS NULL` mismatch on absent cells, recheck the bit-index formula in `appendRecord`'s optional branch against the bit-push order in `add()`'s optional branch.

- [ ] **Step 10: Commit**

```bash
git add include/llamaduck.h src/direntbatch.cpp test/test_dbbatch_binary.cpp meson.build
git commit -m "feat(llamaduck): DBBatch support for fixed-width binary columns"
```

---

## Task 4: `FieldHash` type flip

**Files:**
- Modify: `include/fieldhash.h` (line 9 — the `hash` member)
- Modify: any `.cpp` site that constructs / reads `FieldHash::hash` as a raw `uint8_t[32]` (most consumers go through `to_string()` or `operator==`, which both still work)
- Test: `test/test_fieldhasher.cpp`, `test/test_recordhasher.cpp` should remain green

- [ ] **Step 1: Read current FieldHash definition**

Read `include/fieldhash.h` to see the current struct layout and any methods that touch `hash` directly.

- [ ] **Step 2: Flip the member type**

Change `uint8_t hash[32];` to `std::array<uint8_t, 32> hash;`. Add `#include <array>` if not already present.

Verify `to_string()` (used in logging / error messages, per spec line 70) continues to compile — it likely iterates via `hash` which works the same for `std::array`. `operator==` and `operator!=` should continue to work via `std::array`'s built-in comparison.

- [ ] **Step 3: Build, chase any compilation errors**

Run: `meson compile -C build`

Expected error sites (chase each, update to `std::array` idioms):
- Sites doing `std::copy_n(fh.hash, 32, dest)` — replace with `dest = fh.hash` if `dest` is also `std::array<uint8_t, 32>`, otherwise leave the copy.
- Sites doing `memcpy(dest, fh.hash, 32)` — replace with `std::copy(fh.hash.begin(), fh.hash.end(), dest)` or `memcpy(dest, fh.hash.data(), 32)`.
- Sites doing `fh.hash[i]` — keep as-is, `std::array` supports `operator[]`.

After Task 4, consumer fields are still `std::string`, so the existing hex-conversion path through `to_string()` continues.

- [ ] **Step 4: Run tests**

Run: `meson test -C build --verbose`
Expected: all tests pass.

- [ ] **Step 5: Commit**

```bash
git add include/fieldhash.h <any .cpp files chased>
git commit -m "refactor(fieldhash): hash member is std::array<uint8_t, 32>"
```

---

## Task 5: Flip `Inode.Id` (part of Task 9's atomic commit)

> **DO NOT COMMIT STANDALONE.** Tasks 5-9 form a single atomic group: all hash-derived ID columns share FK relationships and must flip together to preserve runtime consistency. Tasks 5-8 are scoped sub-units describing which sites to touch; Task 9 absorbs their work and is the single commit point. Execute Tasks 5-8 as in-place edits, then proceed to Task 9 for tests and commit.

`include/inode.h:25`: `std::string Id` → `std::array<uint8_t, 32> Id`. This breaks every producer/consumer of `Inode.Id`; subsequent sub-tasks and Task 9 fix the rest.

**Files:**
- Modify: `include/inode.h` (line 25)
- Modify: `src/tskreader.cpp` (line 152 — `inode.Id` assignment from `FieldHash`)
- Modify: `src/posixreader.cpp` (line 135 — zero-array sentinel)
- Modify: `src/recordhasher.cpp` (any site that constructs `Inode.Id` from a hex string)
- Modify: inode-batch SQL emission site (DBBatch<Inode> calls; the column type is now BLOB, and the new llamaduck.h branches handle it)
- Test: `test/test_tskreader.cpp`, `test/test_posixreader.cpp`, `test/test_recordhasher.cpp` — update any assertion that compares `Inode.Id` against a hex string

- [ ] **Step 1: Read current Inode struct and find consumers**

Read `include/inode.h`. Then grep for `Inode\b\.Id\|->Id\b` (scoped to inode usage) across `src/` and `test/`.

Run: `grep -rn 'inode\.Id\|Inode\.Id\|\.InodeId' /Users/jon/code/llama/src /Users/jon/code/llama/test`

Expected: a handful of producer / consumer sites. Make a mental list before editing.

- [ ] **Step 2: Flip the field type**

In `include/inode.h:25`, change `std::string Id;` to `std::array<uint8_t, 32> Id;`. Add `#include <array>` if not present.

- [ ] **Step 3: Fix tskreader.cpp:152**

Per spec, `inode.Id = RecHasher.hashInode(inode).hash;` — direct `std::array` assignment.

Do NOT touch `tskreader.cpp:176` (`entry->InodeId = inode.Id;`) in this task. `Entry.InodeId` flips in Task 9 as part of the atomic HashRec/FFI integration task; the line 176 edit ships there. If the line 176 site fails to compile after this task because the RHS is now array but the LHS is still string, see Step 7 below for the suppression strategy.

- [ ] **Step 4: Fix posixreader.cpp:135 with zero-array sentinel**

```cpp
inode.Id = {};   // all-zero 32-byte sentinel
```

- [ ] **Step 5: Fix recordhasher.cpp**

If any site constructed `Inode.Id` from a hex string of the FieldHash, replace with direct `std::array` assignment from `FieldHash::hash` (after Task 4, both are `std::array<uint8_t, 32>`).

- [ ] **Step 6: Update tests**

Any test that asserts on the hex form of `Inode.Id` switches to comparing bytes:

```cpp
// before:
REQUIRE(inode.Id == "abc123...");
// after:
const std::array<uint8_t, 32> expected{0xab, 0xc1, 0x23, ...};
REQUIRE(inode.Id == expected);
```

If a test wants to assert "Inode.Id is the all-zero sentinel", use:

```cpp
REQUIRE(inode.Id == std::array<uint8_t, 32>{});
```

- [ ] **Step 7: DO NOT BUILD OR COMMIT YET — continue to Task 6**

Task 9's atomic commit takes care of build and test. Proceed to Task 6 with the in-progress changes.

---

## Task 6: Flip `Extent.InodeId` (part of Task 9's atomic commit)

> **DO NOT COMMIT STANDALONE.** See banner in Task 5.

**Files:**
- Modify: `include/extent.h` (line 25)
- Modify: producers — `src/posixreader.cpp:171`, `src/tskreader.cpp` (extent creation site if it sets `InodeId`)
- Test: `test/test_extent.cpp` (if it asserts on `InodeId` type/value)

- [ ] **Step 1: Read extent.h and grep `Extent.InodeId` consumers**

Run: `grep -rn 'Extent\|\.InodeId' /Users/jon/code/llama/src/posixreader.cpp /Users/jon/code/llama/src/tskreader.cpp /Users/jon/code/llama/test/test_extent.cpp`

- [ ] **Step 2: Flip the type**

`include/extent.h:25`: `std::string InodeId;` → `std::array<uint8_t, 32> InodeId;`. Add `#include <array>`.

- [ ] **Step 3: Fix producers**

`posixreader.cpp:171`: `ext.InodeId = {};` (zero-array sentinel).

`tskreader.cpp` extent creation: `ext.InodeId = inode.Id;` (direct `std::array` copy now that both are arrays after Task 5).

- [ ] **Step 4: Update tests**

Same pattern as Task 5 — any assertion on hex string switches to byte-array assertion.

- [ ] **Step 5: DO NOT BUILD OR COMMIT YET — continue to Task 7**

---

## Task 7: Flip Dirent ID columns (part of Task 9's atomic commit)

> **DO NOT COMMIT STANDALONE.** See banner in Task 5.

**Files:**
- Modify: `include/direntbatch.h` (lines 24, 37, 38 — `Id`, `MetaId`, `ParentId`)
- Modify: `src/direntstack.cpp` (lines 34, 38, 41 — three assignments)
- Modify: `src/tskimgassembler.cpp` (lines 90, 111-114 — `RootInodeId` init, `setCurrentRootInodeId` signature, emptiness check)
- Test: `test/test_direntstack.cpp`, `test/test_tskimgassembler.cpp`, `test/test_dirconversion.cpp` — update any hex-string assertions on Dirent IDs

- [ ] **Step 1: Read and grep**

Read `include/direntbatch.h:20-50` to see the Dirent struct in context.

Run: `grep -rn '\.Id\b\|\.MetaId\b\|\.ParentId\b\|setCurrentRootInodeId' /Users/jon/code/llama/src /Users/jon/code/llama/test | grep -i dirent`

- [ ] **Step 2: Flip the three Dirent fields**

In `include/direntbatch.h`:
- Line 24: `std::array<uint8_t, 32> Id;`
- Line 37: `std::array<uint8_t, 32> MetaId;`
- Line 38: `std::array<uint8_t, 32> ParentId;`

Add `#include <array>`.

- [ ] **Step 3: Fix direntstack.cpp**

```cpp
// Line 34:
rec.Id = RecHasher.hashDirent(rec).hash;
// Line 38:
rec.MetaId = RecHasher.hashInodeIdentity(/*…*/).hash;
// Line 41:
rec.ParentId = RecHasher.hashInodeIdentity(/*…*/).hash;
```

- [ ] **Step 4: Fix tskimgassembler.cpp**

Line 90 — initialise `RootInodeId` as `std::array<uint8_t, 32>{}` (zero-filled).

Lines 111-114 — `setCurrentRootInodeId(const std::array<uint8_t, 32>& id)` signature; emptiness check switches from `std::string::empty()` to `id == std::array<uint8_t, 32>{}`.

- [ ] **Step 5: Update tests**

Same pattern as Task 5.

- [ ] **Step 6: DO NOT BUILD OR COMMIT YET — continue to Task 8**

---

## Task 8: Flip `FilesystemRec` IDs (part of Task 9's atomic commit)

> **DO NOT COMMIT STANDALONE.** See banner in Task 5.

**Files:**
- Modify: `include/evidencerec.h` (lines 98, 99 — `RootDirentId`, `RootInodeId`)
- Modify: `src/tskimgassembler.cpp` (already partially touched in Task 7; ensure `RootDirentId` / `RootInodeId` writes use byte arrays)
- Test: `test/test_duckdb.cpp:460` (existing `DBBatch<FilesystemRec>` test) must remain green

- [ ] **Step 1: Flip the two fields**

In `include/evidencerec.h:98-99`:
- `std::array<uint8_t, 32> RootDirentId;`
- `std::array<uint8_t, 32> RootInodeId;`

- [ ] **Step 2: Update producers**

`src/tskimgassembler.cpp`: ensure writes into `FilesystemRec.RootDirentId` / `RootInodeId` use byte arrays. Likely the values flow from `Dirent.Id` (Task 7) and `Inode.Id` (Task 5), which are already arrays — direct assignment.

- [ ] **Step 3: DO NOT BUILD OR COMMIT YET — continue to Task 9**

---

## Task 9: Final atomic commit — HashRec reshape + Entry.InodeId + remaining batch fields + plugin FFI + tests

> **This is the build-green / commit point for Tasks 5-9 combined.** All work from Tasks 5, 6, 7, 8 is staged in the working tree at this point; this task adds the remaining `Entry.InodeId`, `RuleHit`, `SearchHit`, `FileSigResult`, `HashRec`, and plugin FFI flips, then verifies the build and tests for the full atomic group, then commits everything.

This is the integration task. One field flip, three batch-field flips, one struct reshape, and the FFI struct are all interdependent through `src/processor.cpp` and must land in a single commit:

- `Entry.InodeId` (entry.h:18) flips to `std::array<uint8_t, 32>`.
- `RuleHit.inode_id` and `SearchHit.file_hash` (llamabatch.h:27,38) flip to `std::array<uint8_t, 32>`.
- `FileSigResult.FileHash` (ducksig.h:20) flips to `std::array<uint8_t, 32>`.
- `HashRec` (duckhash.h) reshapes: `InodeId` becomes `std::array<uint8_t, 32>`; `MD5`/`SHA1`/`SHA256`/`Blake3` become `std::optional<std::array<uint8_t, N>>`; `Ssdeep` stays `std::string`. `HashRec::set` rewrites without hex encoding.
- `processor.h::setSHA256` signature flips.
- `plugin_api.h::LlamaFileContext.sha256` flips to inline `uint8_t[32]`.
- All `src/processor.cpp` call sites (143, 162, 170-180, 296) update to optional-dereference + raw-byte semantics.

The dependency cycle that forces a single commit:
- processor.cpp:143 calls `HashRecord.set(h, entry.InodeId, HashAlgs)` — needs `entry.InodeId` array AND `HashRec::set` taking array.
- processor.cpp:296 constructs `SearchHit{..., *HashRecord.SHA256, ...}` — needs `HashRec.SHA256` optional AND `SearchHit.file_hash` array.
- processor.cpp:162 constructs `FileSigResult{*HashRecord.SHA256, ...}` — needs `HashRec.SHA256` optional AND `FileSigResult.FileHash` array.
- processor.cpp:170-180 memcpys `HashRecord.SHA256->data()` into `pluginCtx.sha256` — needs `HashRec.SHA256` optional AND `plugin_api.h::sha256` inline bytes.

No intermediate state of fewer-than-all of these is build-green.

**Files:**
- Modify: `include/entry.h:18` (`Entry.InodeId`)
- Modify: `include/llamabatch.h` (line 27 — `RuleHit.inode_id`; line 38 — `SearchHit.file_hash`)
- Modify: `include/ducksig.h` (line 20 — `FileSigResult.FileHash`)
- Modify: `include/duckhash.h` (full `HashRec` reshape; add `toArray<N>` and `ssdeepText` helpers)
- Modify: `include/processor.h:80` (`setSHA256` signature)
- Modify: `include/plugin_api.h:33` (`sha256` type flip + doc comment update)
- Modify: `src/processor.cpp` (lines 143, 162, 170-180 plugin dispatch, 296, plus `setSHA256` impl)
- Modify: `src/tskreader.cpp:176` (`entry->InodeId = inode.Id` — both arrays now)
- Modify: any other producers of `Entry` / `RuleHit` / `SearchHit` / `FileSigResult` that the compiler points to
- Test: `test/test_duckdb.cpp` (HashRec round-trip), `test/test_processor.cpp:61` (`DBBatch<SearchHit>`), `test/test_filesignatures.cpp`, `test/test_ruleengine.cpp`, `test/test_pluginmanager.cpp`

- [ ] **Step 1: Read all touched files first**

```bash
grep -n 'setSHA256\|sha256\|SHA256' /Users/jon/code/llama/include/processor.h /Users/jon/code/llama/src/processor.cpp /Users/jon/code/llama/include/plugin_api.h
```

Read `include/entry.h`, `include/duckhash.h`, `include/llamabatch.h`, `include/ducksig.h`, `include/plugin_api.h`, and `src/processor.cpp` lines 140-180 and 290-300 to understand the existing flow.

```bash
grep -rn 'RuleHit\|SearchHit\|FileSigResult\|inode_id\b\|file_hash\b' /Users/jon/code/llama/src /Users/jon/code/llama/test
grep -rn 'Entry\.InodeId\|entry\.InodeId\|entry->InodeId' /Users/jon/code/llama/src /Users/jon/code/llama/test
```

- [ ] **Step 2: Flip `entry.h::Entry.InodeId`**

`include/entry.h:18`: `std::string InodeId;` → `std::array<uint8_t, 32> InodeId;`. Add `#include <array>`.

`src/tskreader.cpp:176`: `entry->InodeId = inode.Id;` is now a direct `std::array` copy (both sides are arrays after Task 5 and this step).

- [ ] **Step 3: Flip `llamabatch.h` fields**

In `include/llamabatch.h`:
- Line 27: `std::array<uint8_t, 32> inode_id;`
- Line 38: `std::array<uint8_t, 32> file_hash;`

Add `#include <array>`.

- [ ] **Step 4: Flip `ducksig.h::FileSigResult.FileHash`**

In `include/ducksig.h:20`: `std::array<uint8_t, 32> FileHash;`. Add `#include <array>`.

- [ ] **Step 5: Reshape `HashRec` and add helpers**

Replace the existing `HashRec` (and its `set()` method) in `include/duckhash.h` with the spec's version (spec lines 98-128):

```cpp
#pragma once

#include <array>
#include <optional>
#include <string>
#include <cstdint>
#include <cstring>
#include <hasher.h>  // SFHASH_HashValues / SFHASH_* constants

template<size_t N>
std::array<uint8_t, N> toArray(const uint8_t* src) {
  std::array<uint8_t, N> dst;
  std::memcpy(dst.data(), src, N);
  return dst;
}

inline std::string ssdeepText(const char* fuzzy) {
  // ssdeep is a NUL-terminated C string within its fixed Fuzzy buffer.
  return std::string(fuzzy);
}

struct HashRec {
  static constexpr auto ColNames = {"InodeId", "MD5", "SHA1", "SHA256", "Blake3", "Ssdeep"};

  std::array<uint8_t, 32>                InodeId;          // FK to inode.Id, NOT NULL
  std::optional<std::array<uint8_t, 16>> MD5;
  std::optional<std::array<uint8_t, 20>> SHA1;
  std::optional<std::array<uint8_t, 32>> SHA256;
  std::optional<std::array<uint8_t, 32>> Blake3;
  std::string                            Ssdeep;            // "" when absent

  void set(const SFHASH_HashValues& h,
           const std::array<uint8_t, 32>& inodeId,
           uint64_t hashAlgs) {
    InodeId = inodeId;
    MD5    = (hashAlgs & SFHASH_MD5)       ? std::optional{toArray<16>(h.Md5)}      : std::nullopt;
    SHA1   = (hashAlgs & SFHASH_SHA_1)     ? std::optional{toArray<20>(h.Sha1)}     : std::nullopt;
    SHA256 = (hashAlgs & SFHASH_SHA_2_256) ? std::optional{toArray<32>(h.Sha2_256)} : std::nullopt;
    Blake3 = (hashAlgs & SFHASH_BLAKE3)    ? std::optional{toArray<32>(h.Blake3)}   : std::nullopt;
    Ssdeep = (hashAlgs & SFHASH_FUZZY)     ? ssdeepText(h.Fuzzy)                    : std::string{};
  }
};
```

Remove the old `hexEncode`-based `set()`.

- [ ] **Step 6: Update `processor.h::setSHA256` signature**

Line 80: `void setSHA256(const std::array<uint8_t, 32>& sha);` (or whatever parameter name matches the surrounding context).

Update the implementation in `src/processor.cpp` correspondingly.

- [ ] **Step 7: Update plugin_api.h**

`include/plugin_api.h:33`: change `const char* sha256;` to `uint8_t sha256[32];`. The `struct_size` field (if present) is computed via `sizeof` and updates automatically.

Update the adjacent doc comment to read: "Always populated; the host guarantees SHA-256 is computed for every dispatched inode."

- [ ] **Step 8: Fix all processor.cpp call sites**

Line 143: `HashRecord.set(h, entry.InodeId, HashAlgs);` — `entry.InodeId` is now `std::array<uint8_t, 32>` (Step 2 above), matches the new signature. No further change.

Line 162: `FileSigs->add(FileSigResult{*HashRecord.SHA256, sig->Id});` — `*HashRecord.SHA256` is `std::array<uint8_t, 32>`, matches `FileSigResult.FileHash`.

Lines 170-180 (plugin dispatch block): replace whatever currently copies a hex string into `pluginCtx.sha256`. Per spec:

```cpp
std::memcpy(pluginCtx.sha256, HashRecord.SHA256->data(), 32);
```

(Safe because the baseline `getSupportedHashAlgsFromContext()` unconditionally enables SHA-256 — see spec "Plugin FFI invariant" section.)

Line 296: `SearchHits->add(SearchHit{..., *HashRecord.SHA256, ...});` — `*HashRecord.SHA256` is `std::array<uint8_t, 32>`, matches `SearchHit.file_hash`.

- [ ] **Step 9: Update producers of Entry / RuleHit / SearchHit / FileSigResult that the compiler points to**

Beyond the call sites above, build errors will surface any other producer (rule-engine emission paths, test fixtures, other Entry consumers). Fix each as the compiler reports them. Common pattern: direct `std::array` assignment from a producing `std::array` field; `*HashRecord.SHA256` dereference where SHA-256 bytes are needed.

- [ ] **Step 10: Update tests**

In `test/test_duckdb.cpp` (HashRec round-trip): the `DBBatch<HashRec>` round-trip now produces:
- `InodeId` as BLOB length 32, NOT NULL
- `MD5` as BLOB length 16, nullable
- `SHA1` as BLOB length 20, nullable
- `SHA256`, `Blake3` as BLOB length 32, nullable
- `Ssdeep` as VARCHAR (unchanged)

Update assertions to compare bytes (or hex via `lower(hex(col))`) instead of raw hex strings, and to test `IS NULL` on absent optionals.

In `test/test_processor.cpp:61` (`DBBatch<SearchHit>` test): update fixture data to use `std::array` for `file_hash`.

In `test/test_filesignatures.cpp`: update assertions on `FileSigResult.FileHash`.

In `test/test_ruleengine.cpp`: update assertions on `RuleHit.inode_id`.

In `test/test_pluginmanager.cpp`: if any test-side plugin shim populated `LlamaFileContext.sha256` from a hex string, update it to populate raw bytes.

- [ ] **Step 11: Build and run all tests**

Run: `meson compile -C build`
Expected: build green.

Run: `meson test -C build --verbose`
Expected: all tests pass.

If the build fails at an unfamiliar site, it's likely another consumer of `Entry` / `RuleHit` / `SearchHit` / `FileSigResult` / `HashRec` / `pluginCtx.sha256` that wasn't enumerated. Fix as needed and add the file to the commit.

- [ ] **Step 12: Commit**

```bash
git add \
  include/inode.h include/entry.h include/extent.h include/direntbatch.h \
  include/evidencerec.h include/llamabatch.h include/ducksig.h \
  include/duckhash.h include/processor.h include/plugin_api.h \
  src/tskreader.cpp src/posixreader.cpp src/recordhasher.cpp \
  src/direntstack.cpp src/tskimgassembler.cpp src/processor.cpp \
  test/test_tskreader.cpp test/test_posixreader.cpp test/test_recordhasher.cpp \
  test/test_extent.cpp test/test_direntstack.cpp test/test_tskimgassembler.cpp \
  test/test_dirconversion.cpp test/test_duckdb.cpp test/test_processor.cpp \
  test/test_filesignatures.cpp test/test_ruleengine.cpp test/test_pluginmanager.cpp
git commit -m "feat: flip all hash and ID columns to BLOB / std::array<uint8_t, N>"
```

---

## Task 10: Rule parser hash-literal handling (verify scope)

The spec calls out `src/parser.cpp` and surrounding rule-compile sites for `unhex`-at-parse-time of hash literals so downstream comparisons work against BLOB columns. Inspection (during plan writing) showed `FileHashRecord` is parsed into `vector<pair<alg, string_view>>` (parser.h:144) but never reaches SQL emission today — `llama.cpp:432` and `querybuilder.cpp:138` JOIN column-to-column on `h.SHA256 = fs.FileHash`, not against rule-parsed literals. This task verifies that assumption and, if confirmed, becomes a no-op (with a note left in `parser.h`).

**Files:**
- Modify (only if needed): `include/parser.h` (line 144 — `FileHashRecord` element type), `src/parser.cpp` (parsing site)
- Test: existing rule-engine tests must remain green

- [ ] **Step 1: Confirm no SQL emission path consumes `FileHashRecord`**

Run: `grep -rn 'FileHashRecord\|FileHashRecords\|hashSection\.Hash\b' /Users/jon/code/llama/src`

Expected (per inspection during plan writing): `FileHashRecord` is populated by the parser, stored on `HashSection`, but never read by any SQL emitter. Confirm this is still true.

Also run: `grep -rn 'rule.Hash\|\.Hash\b' /Users/jon/code/llama/src` to scan rule-engine consumers more broadly.

- [ ] **Step 2: If confirmed no consumer, document and skip**

In `include/parser.h:144`, add a one-line comment above the `using FileHashRecord = ...` declaration:

```cpp
// Parsed but not currently emitted into SQL. When wired to SQL,
// decode the hex string_view into bytes here so comparisons can
// target BLOB columns directly. See spec
// docs/superpowers/specs/2026-05-25-binary-hash-migration-design.md.
using FileHashRecord = std::vector<std::pair<SFHASH_HashAlgorithm, std::string_view>>;
```

Commit and proceed.

- [ ] **Step 3: If a consumer exists, decode at parse time**

If grep surfaces a SQL-emission consumer, change `FileHashRecord` to carry a decoded byte representation. Concrete shape (suggested):

```cpp
struct HashLiteral {
  SFHASH_HashAlgorithm alg;
  std::vector<uint8_t> bytes;  // hex-decoded from parser.cpp:139 parseHashValue()
};
using FileHashRecord = std::vector<HashLiteral>;
```

Update `parser.cpp:139` `parseHashValue()` to return decoded bytes (using existing hex decoding utility — check `test/test_hex.cpp` for the helper it tests). Update consumers to emit `unhex('...')` SQL or to pass raw bytes into a parameterised query.

- [ ] **Step 4: Build and test**

Run: `meson compile -C build`
Run: `meson test -C build --verbose`
Expected: build green, tests pass.

- [ ] **Step 5: Commit**

```bash
git add include/parser.h <other touched files>
git commit -m "docs(parser): FileHashRecord byte-decode is deferred until a SQL consumer exists"
```

(or, if Step 3 was needed:)

```bash
git commit -m "feat(parser): decode rule hash literals to bytes at parse time"
```

---

## Task 11: Documentation update

**Files:**
- Modify: `docs/core/database-schema.md`

- [ ] **Step 1: Read current schema doc**

Read `docs/core/database-schema.md`.

- [ ] **Step 2: Flip column types VARCHAR → BLOB**

For every cryptographic-hash column and every hash-derived 32-byte ID column listed in the schema, change the documented type from VARCHAR to BLOB. Per spec, that covers (non-exhaustively):
- `Inode.Id`
- `Dirent.Id`, `Dirent.MetaId`, `Dirent.ParentId`
- `Extent.InodeId`
- `RuleHit.inode_id`
- `SearchHit.file_hash`
- `FileSigResult.FileHash`
- `FilesystemRec.RootDirentId`, `FilesystemRec.RootInodeId`
- `HashRec.InodeId`, `HashRec.MD5`, `HashRec.SHA1`, `HashRec.SHA256`, `HashRec.Blake3`
- `HashRec.Ssdeep` stays VARCHAR (nullable; `""` when absent)
- `EvidenceFileRec.VerificationHash` stays VARCHAR (out of scope per spec non-goals)

- [ ] **Step 3: Add idiom paragraph**

Add a section near the schema describing ad-hoc querying conventions:

```markdown
### Working with BLOB hash columns

DuckDB BLOB columns store raw bytes. For lookups by a known hex hash,
use `unhex()`:

    SELECT * FROM hashes
    WHERE SHA256 = unhex('e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855');

For human-readable output, hex-encode with `lower(hex(...))`:

    SELECT lower(hex(SHA256)) AS sha256_hex FROM hashes;

BLOB↔BLOB joins (the FK pattern across `Inode.Id`, `Dirent.MetaId`,
etc.) work transparently — equality is bytewise.
```

- [ ] **Step 4: Update schema diagram if present**

If the doc contains a Mermaid / ASCII schema diagram that reproduces column types, sync it with the type flip.

- [ ] **Step 5: Build (to ensure no doc-driven tests break)**

Run: `meson compile -C build`
Run: `meson test -C build --verbose`

- [ ] **Step 6: Commit**

```bash
git add docs/core/database-schema.md
git commit -m "docs(schema): hash and ID columns are BLOB; add unhex/hex idiom notes"
```

---

## Wrap-up checks

After Task 11, run the full suite one more time to confirm nothing regressed across the migration:

- [ ] **Final build**: `meson compile -C build` (clean green)
- [ ] **Final test**: `meson test -C build --verbose` (all green)
- [ ] **Spot-check a real DB export** (optional but recommended for a migration of this size): run the CLI against a small test image and `duckdb` the resulting parquet to verify hash columns appear as BLOB with the expected lengths:

```sql
SELECT typeof(SHA256), length(SHA256), lower(hex(SHA256)) FROM 'hashes.parquet' LIMIT 3;
```

Expected: `BLOB | 32 | <64 hex chars>` per row.

- [ ] **Confirm FFI doc**: the comment above `LlamaFileContext.sha256` in `include/plugin_api.h` reads "always populated; the host guarantees SHA-256 is computed for every dispatched inode."
