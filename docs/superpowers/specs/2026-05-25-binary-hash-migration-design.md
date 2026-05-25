# Binary Hash Migration — llama side

Flip every cryptographic-hash column and every hash-derived 32-byte ID
column in llama's DuckDB / parquet schema from `VARCHAR` (hex) to
`BLOB` (raw bytes), change the plugin FFI's `LlamaFileContext.sha256`
field from a hex `const char*` to an inline `uint8_t[32]`, and extend
`llamaduck.h`'s `boost::pfr`-driven type mapping to support
fixed-width binary columns with optional / nullable variants.

## Cross-repo context

llama is the base of a coordinated multi-repo migration. It lands
standalone first. findallevidence is covered by a separate spec
executed after llama is in good shape. keywitness-core has its own
parallel migration of unrelated column types.

llama and findallevidence hand-mirror an FFI struct (`LlamaFileContext`)
with no build-time check enforcing they match. Both are pre-alpha and
private; the broken intermediate state between llama's PR landing and
findallevidence's PR landing is acceptable.

## Goals

- Single canonical representation for every cryptographic hash
  column: `BLOB`, of natural width per algorithm (16 for MD5, 20 for
  SHA-1, 32 for SHA-256 and BLAKE3). Ssdeep is a variable-length fuzzy
  hash and stays as text (nullable VARCHAR).
- Single canonical representation for every hash-derived 32-byte ID
  column (`Inode.Id`, `Dirent.Id`, and every FK column referencing
  them): `BLOB`, exactly 32 bytes — the raw `FieldHash::hash` value
  with no hex round-trip.
- `LlamaFileContext.sha256` FFI field carries raw bytes
  (`uint8_t sha256[32]`).
- `llamaduck.h`'s `DBBatch<T>` reflection-driven appender handles
  `std::array<uint8_t, N>` and `std::optional<std::array<uint8_t, N>>`
  natively, with the same `boost::pfr`-based ergonomics as today's
  integer and string columns. Existing `std::string` columns continue
  to use `""` as the absent-sentinel; no nullable-string plumbing is
  added.
- Plugin FFI documentation states that `LlamaFileContext.sha256` is
  always populated. The guarantee is provided by
  `ProcessorContext::getSupportedHashAlgsFromContext()`, which
  unconditionally includes `SFHASH_SHA_2_256` in the baseline algorithm
  set. No runtime guard is added in this migration; if a future change
  introduces a CLI-driven `--hash` flag that could disable SHA-256,
  that change adds the corresponding guard.

## Non-goals

- Backwards compatibility with parquet archives produced before this
  migration. Pre-alpha and private; existing data is throwaway. No
  migration tool, no transitional dual schemas.
- View layers, hex-rendering DuckDB macros, or generated `*_hex`
  columns. Documentation only.
- Llama rule-language grammar changes. Rule files keep hex hash
  literals; the rule compiler hex-decodes to bytes at parse time.
- Fixing PosixReader's missing `inode.Id`. Tracked separately. Until
  resolved, PosixReader writes the all-zero 32-byte sentinel (the
  binary analogue of today's empty-string sentinel).
- Flipping `EvidenceFileRec.VerificationHash`. Not joined against
  anything; leaves as VARCHAR until separately motivated.
- Changing `dirent.Id` *semantics* — the `2026-05-23-inode-id-primary-key-design.md`
  non-goal still stands. Only its representation flips.

## Change surface

| Site | Change |
| --- | --- |
| `include/fieldhash.h:9` | `uint8_t hash[32]` becomes `std::array<uint8_t, 32> hash`. Enables direct assignment into the (also `std::array`-typed) `Id` fields without `std::copy_n` ceremony. `to_string()` stays (used for logging / error messages); `operator==`/`!=` keep working. |
| `include/inode.h:25` | `std::string Id` → `std::array<uint8_t, 32> Id`. |
| `include/duckhash.h` | `HashRec` reshaped — see dedicated section. |
| `include/direntbatch.h:24,37,38` | `Dirent.Id`, `Dirent.MetaId`, `Dirent.ParentId` flip to `std::array<uint8_t, 32>`. |
| `include/extent.h:25` | `Extent.InodeId` flips to `std::array<uint8_t, 32>`. |
| `include/llamabatch.h:27,38` | `RuleHit.inode_id` flips to `std::array<uint8_t, 32>`; `SearchHit.file_hash` flips to `std::array<uint8_t, 32>`. |
| `include/ducksig.h:20` | `FileSigResult.FileHash` flips to `std::array<uint8_t, 32>`. |
| `include/evidencerec.h:98,99` | `FilesystemRec.RootDirentId` and `RootInodeId` flip to `std::array<uint8_t, 32>`. |
| `include/entry.h:18` | `Entry.InodeId` (in-memory carrier) flips to `std::array<uint8_t, 32>`. |
| `include/llamaduck.h` | New type-trait plumbing for fixed-width binary (optional and non-optional) — see dedicated section. |
| `include/bitsetvar.h` (new) | Growable-bitset utility. See dedicated section. |
| `include/processor.h:80` | `setSHA256(const std::string&)` → `setSHA256(const std::array<uint8_t, 32>&)`. |
| `include/plugin_api.h:33` | `const char* sha256` → `uint8_t sha256[32]` inline; `struct_size` updates automatically via `sizeof`. |
| `src/recordhasher.cpp:122-123` | `hashInode(const Inode&)` and `hashInodeIdentity(...)` already return `FieldHash`. No signature change; consumers stop calling `.to_string()`. |
| `src/tskreader.cpp:152,176` | `inode.Id = RecHasher.hashInode(inode).hash` (assign `std::array` directly). `entry->InodeId = inode.Id` (copy `std::array`). |
| `src/direntstack.cpp:34,38,41` | `rec.Id = RecHasher.hashDirent(rec).hash`; `rec.MetaId = RecHasher.hashInodeIdentity(...).hash`; `rec.ParentId = RecHasher.hashInodeIdentity(...).hash`. |
| `src/tskimgassembler.cpp:90,111-114` | `RootInodeId` initialised to all-zero array; `setCurrentRootInodeId(const std::array<uint8_t, 32>&)`; emptiness check is "all bytes zero" rather than `std::string::empty()`. |
| `src/posixreader.cpp:135,171` | `inode.Id = {}` and `ext.InodeId = {}` (zero-filled 32-byte sentinel). |
| `src/processor.cpp:143` | `HashRecord.set(h, entry.InodeId, HashAlgs)` — caller already passes the right type after `Entry.InodeId` flips. |
| `src/processor.cpp:162` | `FileSigs->add(FileSigResult{*HashRecord.SHA256, sig->Id})` — safe because the baseline algorithm set in `getSupportedHashAlgsFromContext()` unconditionally enables SHA-256. |
| `src/processor.cpp:170-180` (plugin dispatch block) | `std::memcpy(pluginCtx.sha256, HashRecord.SHA256->data(), 32)`. |
| `src/processor.cpp:296` | `SearchHits->add(SearchHit{..., *HashRecord.SHA256, ...})`. |
| `src/parser.cpp` and surrounding rule-compile sites | At rule-compile time, `unhex` the textual hash literal into a fixed-width byte array so downstream comparisons work against BLOB columns. |
| `src/llama.cpp:424-474` and `src/querybuilder.cpp:129-138` | SQL strings unchanged. BLOB↔BLOB equality compares the same as VARCHAR↔VARCHAR. |
| `docs/core/database-schema.md` | All hash-column and ID-column types flip from VARCHAR to BLOB. Idiom note covers `unhex(...)` for ad-hoc lookups and `lower(hex(col))` for human-readable output. |

## `HashRec` and `Dirent` after the flip

```cpp
struct HashRec {
  static constexpr auto ColNames = {"InodeId", "MD5", "SHA1", "SHA256", "Blake3", "Ssdeep"};

  std::array<uint8_t, 32>                            InodeId;          // FK to inode.Id, NOT NULL
  std::optional<std::array<uint8_t, 16>>             MD5;
  std::optional<std::array<uint8_t, 20>>             SHA1;
  std::optional<std::array<uint8_t, 32>>             SHA256;
  std::optional<std::array<uint8_t, 32>>             Blake3;
  std::string                                        Ssdeep;            // "" when absent

  void set(const SFHASH_HashValues& h,
           const std::array<uint8_t, 32>& inodeId,
           uint64_t hashAlgs);
};
```

`HashRec::set` writes optionals based on the algorithm-bitmask, with
no hex encoding:

```cpp
void HashRec::set(const SFHASH_HashValues& h,
                  const std::array<uint8_t, 32>& inodeId,
                  uint64_t hashAlgs) {
  InodeId = inodeId;
  MD5    = (hashAlgs & SFHASH_MD5)       ? std::optional{toArray<16>(h.Md5)}      : std::nullopt;
  SHA1   = (hashAlgs & SFHASH_SHA_1)     ? std::optional{toArray<20>(h.Sha1)}     : std::nullopt;
  SHA256 = (hashAlgs & SFHASH_SHA_2_256) ? std::optional{toArray<32>(h.Sha2_256)} : std::nullopt;
  Blake3 = (hashAlgs & SFHASH_BLAKE3)    ? std::optional{toArray<32>(h.Blake3)}   : std::nullopt;
  Ssdeep = (hashAlgs & SFHASH_FUZZY)     ? ssdeepText(h.Fuzzy)                    : std::string{};
}
```

`toArray<N>(const uint8_t*)` is a one-line helper that memcpys into an
`std::array<uint8_t, N>`. `ssdeepText` copies until the first NUL
within the buffer (ssdeep is C-string-shaped within its fixed `Fuzzy`
buffer). Both helpers live next to `HashRec` in `duckhash.h`.

`Dirent` keeps its existing fields; only the three hash-derived ID
fields change type:

```cpp
struct Dirent {
  static constexpr auto ColNames = { /* unchanged */ };

  std::array<uint8_t, 32> Id;
  std::string Path;
  std::string Name;
  std::string ShortName;
  std::string Type;
  std::string Flags;
  uint64_t    MetaAddr;
  uint64_t    ParentAddr;
  uint64_t    MetaSeq;
  uint64_t    ParentSeq;
  std::array<uint8_t, 32> MetaId;
  std::array<uint8_t, 32> ParentId;
};
```

## `llamaduck.h` plumbing

The existing `boost::pfr`-based reflection handles two C++ types
today: `uint64_t` → `UBIGINT`, `const char*` / `std::string` →
`VARCHAR`. We extend it to handle two more:

- `std::array<uint8_t, N>` → NOT NULL `BLOB`
- `std::optional<std::array<uint8_t, N>>` → nullable `BLOB`

### Storage model in `DBBatch<T>`

Strings retain their existing storage (`Buf` + `OffsetVals`). Binary
columns get a dedicated, simpler layout that exploits their
compile-time-fixed width:

```cpp
template<typename T>
struct DBBatch {
  std::vector<char>     Buf;          // variable-width (strings, null-terminated)
  std::vector<uint64_t> OffsetVals;   // integers + string offsets; no binary slots
  std::vector<uint8_t>  BinaryBuf;    // R * RowBinaryBytes, zero-filled for nulls
  BitsetVar             BinaryNull;   // R * NumNullableBinaryCols bits
  uint64_t              NumRows = 0;
};
```

Binary columns do not consume `OffsetVals` slots. Row R's bytes for
binary column J live at the compile-time-deducible position
`R * RowBinaryBytes + binaryColOffset(J)` in `BinaryBuf`, regardless
of whether that cell is null. This costs N zero bytes per null cell
(a known sub-optimality when whole hash types are disabled); the
simplicity of the grid layout is the trade-off we accept.

Null status is tracked separately in `BinaryNull`, but **only for
`std::optional<std::array<uint8_t, N>>` columns** — non-optional
`std::array<uint8_t, N>` columns are NOT NULL by C++ type contract
and consume no bitset space. For record types with no optional binary
columns (`Dirent`, `Inode`), the bitset stays empty across the
lifetime of the batch.

Strings (including `Ssdeep`) continue to use the existing `Buf` +
`OffsetVals` path with `""` as the absent-sentinel. No new
nullable-string machinery.

### New type traits

Live next to `duckdbType<T>()` in `llamaduck.h`:

```cpp
template<typename T> struct is_byte_array : std::false_type {};
template<size_t N>   struct is_byte_array<std::array<uint8_t, N>> : std::true_type {};
template<typename T> inline constexpr bool is_byte_array_v = is_byte_array<T>::value;

template<typename T> struct byte_array_size : std::integral_constant<size_t, 0> {};
template<size_t N>   struct byte_array_size<std::array<uint8_t, N>> : std::integral_constant<size_t, N> {};
template<typename T> inline constexpr size_t byte_array_size_v = byte_array_size<T>::value;

template<typename T> struct is_optional_byte_array : std::false_type {};
template<size_t N>   struct is_optional_byte_array<std::optional<std::array<uint8_t, N>>> : std::true_type {};
template<typename T> inline constexpr bool is_optional_byte_array_v = is_optional_byte_array<T>::value;
```

### `duckdbType<T>()` extension

```cpp
else if constexpr (is_byte_array_v<T> || is_optional_byte_array_v<T>) {
    return "BLOB";
}
```

NOT NULL discipline is not asserted in the column-type string today
(integer and string branches emit type-only strings, no `NOT NULL`
suffix), so the new branches follow that convention and rely on
DuckDB's default nullability semantics. The C++ type system carries
the contract: `std::array<uint8_t, N>` always present,
`std::optional<...>` may be absent.

### Compile-time per-row binary stride

```cpp
template<typename TupleType, size_t I = 0>
constexpr size_t rowBinaryBytes() {
  if constexpr (I >= std::tuple_size_v<TupleType>) return 0;
  else {
    using C = std::tuple_element_t<I, TupleType>;
    if constexpr (is_byte_array_v<C>)               return byte_array_size_v<C> + rowBinaryBytes<TupleType, I+1>();
    else if constexpr (is_optional_byte_array_v<C>) return byte_array_size_v<typename C::value_type> + rowBinaryBytes<TupleType, I+1>();
    else                                            return rowBinaryBytes<TupleType, I+1>();
  }
}
```

Companion constexpr helpers:

- `binaryColOffset<TupleType, J>()` — cumulative offset within the
  row's binary section, given the binary-column ordinal J. Counts
  both optional and non-optional binary columns (governs `BinaryBuf`
  layout).
- `binaryColIndex<TupleType, CurIndex>()` — count of binary columns
  (optional + non-optional) in the tuple strictly before tuple
  position `CurIndex`.
- `numBinaryCols<TupleType>()` — total binary columns in the tuple
  (optional + non-optional).
- `nullableBinaryColIndex<TupleType, CurIndex>()` — count of
  `std::optional<std::array<uint8_t, N>>` columns in the tuple
  strictly before tuple position `CurIndex`. Governs `BinaryNull`
  bit indexing.
- `numNullableBinaryCols<TupleType>()` — total
  `std::optional<std::array<uint8_t, N>>` columns in the tuple.
- Analogous `nonBinaryColIndex` / `numNonBinaryCols` for indexing
  `OffsetVals` row-major.

The two families differ deliberately: byte storage in `BinaryBuf`
covers all binary columns (because data is written for every cell,
zero-filled when null), while bit storage in `BinaryNull` covers only
optional columns (non-optional cells are never null and need no
bit).

### `add()` and `appendRecord<CurIndex>` extensions

`add(size_t& offset, const Cur& cur)` gains two branches:

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

`appendRecord<CurIndex>` is the trickiest piece. The existing
recursion uses a single backward-walking `index` that decrements by
one per column. After the flip, binary columns no longer consume
`OffsetVals` slots, so the per-column decrement of the `OffsetVals`
cursor must be conditional on the column type.

The cleanest restructuring is to drive the recursion off `row` (the
DBBatch row number) and compute the `OffsetVals` cursor at each step
from compile-time information:

```cpp
template<size_t CurIndex>
void appendRecord(duckdb_appender& appender, size_t row) {
  using ColumnType = typename std::tuple_element<CurIndex, typename DBType<T>::TupleType>::type;

  if constexpr (CurIndex > 0) {
    appendRecord<CurIndex - 1>(appender, row);
  }

  if constexpr (is_byte_array_v<ColumnType>) {
    constexpr size_t N = byte_array_size_v<ColumnType>;
    constexpr size_t colOffset = binaryColOffset<TupleType, binaryColIndex<TupleType, CurIndex>()>();
    appendVal(appender,
              BinaryBuf.data() + row * rowBinaryBytes<TupleType>() + colOffset,
              N);
  }
  else if constexpr (is_optional_byte_array_v<ColumnType>) {
    constexpr size_t N = byte_array_size_v<typename ColumnType::value_type>;
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
  else if constexpr (std::is_integral_v<ColumnType>) {
    const size_t offsetSlot = nonBinaryColumnSlotFor<TupleType, CurIndex>(row);
    appendVal(appender, OffsetVals[offsetSlot]);
  }
  else if constexpr (std::is_convertible_v<ColumnType, std::string>) {
    const size_t offsetSlot = nonBinaryColumnSlotFor<TupleType, CurIndex>(row);
    appendVal(appender, Buf.data() + OffsetVals[offsetSlot]);
  }
}
```

`nonBinaryColumnSlotFor<TupleType, CurIndex>(row)` returns
`row * numNonBinaryCols<TupleType>() + nonBinaryColIndex<TupleType, CurIndex>()`,
where the helpers are constexpr and count non-binary tuple positions
strictly before `CurIndex`. The `copyToDB` loop simplifies
accordingly:

```cpp
size_t copyToDB(duckdb_appender& appender) {
  for (size_t row = 0; row < NumRows; ++row) {
    duckdb_appender_begin_row(appender);
    appendRecord<DBType<T>::NumCols - 1>(appender, row);
    duckdb_appender_end_row(appender);
  }
  return NumRows;
}
```

### New `appendVal` overload

In `src/direntbatch.cpp` (which owns the existing two overloads):

```cpp
void appendVal(duckdb_appender& appender, const uint8_t* data, size_t len) {
    duckdb_state state = duckdb_append_blob(appender, data, len);
    THROW_IF(state == DuckDBError, "duckdb_append_blob failed");
}
```

### Default-constructor caveat (`llamaduck.h:157`)

`std::array<uint8_t, N>` and `std::optional<...>` are both
default-constructible (the array zero-initialises; the optional
defaults to empty). The migration does not worsen the existing
caveat.

## `BitsetVar` (new header `include/bitsetvar.h`)

A growable-bitset utility used by `DBBatch<T>::BinaryNull` and
available for reuse elsewhere.

```cpp
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

Unit tests in their own file cover empty / `push_back` to size 1, 7,
8, 9, 17 (byte-boundary cases); `set` flipping a bit on/off; `get`
round-trip; `clear` resets size and storage; `reserve(n)` allocates
`(n+7)/8` bytes without changing observable state.

## Plugin FFI invariant

After the migration, `HashRecord.SHA256` is `std::optional<...>` and
`LlamaFileContext.sha256` carries 32 raw bytes inline.
`processor.cpp:177` becomes an unconditional `memcpy`:

```cpp
std::memcpy(pluginCtx.sha256, HashRecord.SHA256->data(), 32);
```

The dereference is safe because SHA-256 is unconditionally enabled in
the baseline algorithm set: `ProcessorContext::getSupportedHashAlgsFromContext()`
(`src/processor.cpp:66-67`) starts with
`SFHASH_SHA_2_256 | SFHASH_MD5 | SFHASH_SHA_1` and only OR-extends
from exclusion/inclusion hashsets — there is no path that disables
SHA-256 today, and no CLI flag for one. The plugin FFI documentation
for `LlamaFileContext.sha256` updates to read: "always populated; the
host guarantees SHA-256 is computed for every dispatched inode."

If a future change introduces a CLI-driven `--hash` flag that could
turn SHA-256 off, that change carries the corresponding startup guard
in `Llama::loadPlugins` (or wherever the algorithm set first becomes
visible). It is not part of this migration.

## PosixReader sentinel

`posixreader.cpp:135` and `:171` currently assign `""` as a sentinel
for unknown InodeId. After the flip, the field is
`std::array<uint8_t, 32>`; the sentinel is the all-zero array
(`inode.Id = {};`, `ext.InodeId = {};`).

The existing emptiness-detection callsites switch from
`std::string::empty()` to `std::all_of(arr.begin(), arr.end(), [](uint8_t b){return b == 0;})`
or an equivalent `arr == std::array<uint8_t, 32>{}`. The known
collision risk (all-zero FieldHash being a valid hash output) is
vanishingly improbable for FieldHash inputs and is documented as a
risk below; PosixReader is the only producer of the sentinel and its
fix is tracked separately.

## Testing strategy

- Existing unit tests update where they assert on `std::string` hash
  field types or `VARCHAR` DuckDB columns.
- **Focused `DBBatch<T>` coverage for the `appendRecord` restructuring.**
  The shift from a single backward-walking `index` to row-major
  addressing with constexpr column-ordinal helpers is the second-
  highest-risk change in the migration, and existing batch round-trip
  tests are unlikely to cover the new interleavings. Add fixture
  record-types that exercise each shape independently of the
  production structs:
  - binary at tuple position 0 only
  - binary at the final tuple position only
  - binary at both ends with non-binary in the middle
  - alternating binary / non-binary columns
  - a tuple with only binary columns
  - a tuple with only non-binary columns (regression guard for the
    no-binary-column case)
  - a `std::optional<std::array<uint8_t, N>>` next to a non-optional
    `std::array<uint8_t, M>` (covers the parallel `binaryColIndex` vs
    `nullableBinaryColIndex` counters)
  - multi-row batches with mixed present/absent optionals across
    rows (covers `BinaryNull` bit indexing)
  Each fixture round-trips through `DBBatch::copyToDB` and queries
  DuckDB to confirm `typeof(col) = 'BLOB'`, byte-equality on present
  cells, and `IS NULL` on absent cells.
- **`BitsetVar` unit tests** as described above.
- Existing parquet-export and join-query integration tests should
  pass unchanged — BLOB↔BLOB joins behave identically to VARCHAR↔VARCHAR.

## Documentation impact

`docs/core/database-schema.md` — flip the hash columns' and all
hash-derived ID columns' type from VARCHAR-hex to BLOB. Add a
paragraph showing `unhex('abc...')` for ad-hoc lookups by a known hex
hash, and `lower(hex(sha256))` for human-readable output. Update the
schema diagram if it reproduces column types.

## Risks

- **`appendVal` BLOB overload is the highest-risk single change.**
  Subtle DuckDB Appender misuse (wrong length passed to
  `duckdb_append_blob`, null handling on absent optionals) could
  silently corrupt output. The dedicated DBBatch round-trip unit
  tests cover this. The column type is variable-length `BLOB`, not
  fixed-size `BLOB(N)`.
- **`appendRecord` restructuring is the second-highest risk.** The
  shift from a single backward-walking `index` to row-major addressing
  with constexpr column-ordinal helpers is a real reorganisation, not
  a drop-in extension. Existing tests covering Dirent / Inode /
  HashRec round-trips through DBBatch must all remain green.
- **Hand-mirrored FFI struct with findallevidence.** No build-time
  check; drift surfaces as a runtime segfault in the plugin. Worth
  documenting alongside the FFI struct definition.
- **All-zero 32-byte sentinel collision.** Theoretically a valid
  FieldHash output. Probability is `~2^-256`; documented and
  unaddressed pending PosixReader fix.
- **Wasted bytes in `BinaryBuf` when whole hash types are disabled.**
  Per-row stride × systemically-absent column. For `HashRec` with
  three of four binary hashes disabled, that's ~68 zero bytes per row
  vs. today's ~3 empty VARCHAR cells (~3 bytes). Documented; not
  addressed. A sparse-binary-storage variant in `DBBatch<T>` is a
  follow-up if profiling ever justifies it.

## Follow-ups (out of scope)

- A `BinaryBuf` sparse-storage variant in `DBBatch<T>` if profiling
  shows systemic-absence overhead matters.
- A `totalStringSize` → `totalInlineSize` rename in `llamaduck.h` to
  reflect that the helper now covers both strings and binary stride.
- Flipping `EvidenceFileRec.VerificationHash` to `std::array<uint8_t, 32>`
  if a join motivates it.
- PosixReader InodeId production (already tracked).
