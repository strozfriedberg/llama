# File Signature Analysis Design

## Problem

Llama has a `FileSigAnalyzer` that detects file types via magic byte patterns,
and the rule parser supports `signature:` sections, but the two are not
connected. The analyzer uses `std::ifstream` instead of `ReadSeek`, has an
unnecessary `Check`/`CompareType` subsystem, and is not integrated into the
processing pipeline. Signature detection results are not stored in the database
and the `signature:` section in rules is parsed but never evaluated.

## Scope

1. Add database tables for signature data
2. Rewrite `FileSigAnalyzer` to use `ReadSeek` and Lightgrep-only matching
3. Wire signature detection into the processing pipeline and rule evaluation

Converting the 54 `magics.json` entries that have checks but no patterns is
out of scope and will be handled in a separate workstream.

## Data Model

Two new DuckDB tables, defined in a new `include/ducksig.h` header following
the `duckhash.h` pattern:

**`signatures`** — reference table populated once from `magics.json` at startup:

| Column      | Type    | Description                           |
|-------------|---------|---------------------------------------|
| Id          | VARCHAR | UUID from magics.json                 |
| Name        | VARCHAR | Signature name (e.g., "PDF", "JPEG")  |
| Description | VARCHAR | Longer description                    |

**`file_signatures`** — join table linking files to detected signatures:

| Column   | Type    | Description                        |
|----------|---------|------------------------------------|
| FileHash | VARCHAR | Blake3 hash of the file content    |
| SigId    | VARCHAR | FK to signatures.Id                |

Multiple signatures per file are supported. The structs:

```cpp
struct SigRec {
  static constexpr auto ColNames = {"Id", "Name", "Description"};
  std::string Id;
  std::string Name;
  std::string Description;
};

struct FileSigResult {
  static constexpr auto ColNames = {"FileHash", "SigId"};
  std::string FileHash;
  std::string SigId;
};
```

## FileSigAnalyzer Rewrite

### Removed

- `CompareType` enum, `Check` struct, `OffsetType` struct
- `doCheck()`, `getBuf()`, `Check::compare()`, `parse_compare_type()`
- `Comparator` map, `iequals()`, `str2bin()`, `char2uint8()`, `parseOffset()`
- Extension-based fallback lookup (`SignatureDict`)
- Brute-force `SignatureList` iteration
- `std::ifstream` / `std::filesystem::directory_entry` usage
- Hardcoded `./magics.json` path in constructor

### Retained (with modifications)

- `LightGrep` wrapper class — unchanged, handles program compilation
- `Magic` struct — simplified, retains pattern/encoding/name/id/description
  fields, drops Check-related fields
- `readMagics()` — static, parses magics.json, skips entries without patterns
- `lgSearch()` / `lgCallbackfn()` — modified to collect all matches instead of
  only the lowest-index match
- `getPatternLength()` — unchanged

### New Interface

- Constructor takes `MagicsType` (pre-parsed signatures) and a shared
  `LG_HPROGRAM`. Creates its own `LG_HCONTEXT` for thread-safe searching.
- Primary method: `getSignatures(ReadSeek&, results)` — reads from the
  stream's beginning, runs Lightgrep, returns all matching signatures.
- `readMagics()` remains static and separate from construction.

## Pipeline Integration

### init() — async signature compilation

A new async future in `Llama::init()` parses `magics.json` and compiles the
Lightgrep program for signatures. The compiled program is immutable and stored
as a shared member on `Llama`, analogous to `LgProg`.

### search() — table creation and reference data

After `init()` returns:

- Create the `signatures` and `file_signatures` tables (extend `dbInit()` or
  `RuleEngine::createTables()`)
- Bulk-load the `signatures` reference table from the parsed `MagicsType`
- Pass the shared sig program and `MagicsType` into `ProcessorContext`

### ProcessorContext

Gains a shared pointer to the compiled sig program and the `MagicsType` vector
(needed to map Lightgrep hit indices back to signature metadata).

### Processor

Each `Processor` creates its own `FileSigAnalyzer` from the shared program,
mirroring how the grep `LgCtx` is per-Processor while `Prog` is shared.

New members:
- `LlamaDBAppender` for `file_signatures`
- `DBBatch<FileSigResult>` for batched writes

In `process()`, after hashing: seek to 0, read the signature buffer, run
signature detection, record `FileSigResult` rows for each match.

Flushed alongside hash and search hit batches in `Processor::flush()`.

### QueryBuilder — signature rule evaluation

When `rule.Signature` is non-null, `buildSqlQuery()` adds JOINs to
`file_signatures` and `signatures`, with a WHERE clause generated from the
signature AST using the same recursive `buildSqlClauseImpl` pattern.

A `SignaturePropertySqlLookup` maps rule properties to SQL columns:
- `name` → `signatures.Name`
- `id` → `signatures.Id`

Example generated SQL:

```sql
SELECT '...', Path, Name, Addr FROM dirent, inode
  JOIN file_signatures fs ON hash.Blake3 = fs.FileHash
  JOIN signatures s ON fs.SigId = s.Id
WHERE dirent.Metaaddr == inode.Addr
  AND s.Name = 'PDF'
```

## Future Optimization

A planned fast-follow is evaluating signature sections in `Processor` to
short-circuit grep searches. For example, if all loaded rules target PE files
only, non-PE files can skip pattern matching entirely. This partial-aggregate
rule evaluation for filtering is not part of the current design.

## Testing

- Unit tests for simplified `FileSigAnalyzer` with `ReadSeek` input
- Unit tests for `ducksig.h` batch operations
- Unit tests for `QueryBuilder` signature clause generation
- Integration test: rule with `signature:` section correctly matches files
