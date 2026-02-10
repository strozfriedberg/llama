# Hash Set Analysis Design

## Problem

Llama computes file hashes and has CLI options for exclusion and inclusion hash
sets, but the actual logic is stubbed out with placeholder comments. Additionally,
duplicate files cause spurious writes to the database for content-based results
(hashes, signatures, grep hits). We need to:

1. Implement exclusion and inclusion hash set behavior
2. Deduplicate files by content during processing
3. Benchmark hash algorithms to choose the best primary hash
4. Minimize hash computation via two-phase hashing

## Hash Algorithm Benchmark

Write a Catch2 microbenchmark comparing BLAKE3 vs SHA256 vs MD5 vs SHA1 using
libhasher, across file sizes: 4KB, 64KB, 512KB, 1MB, 8MB.

The winner of BLAKE3 vs SHA256 becomes the **primary hash** — used for dedup and
as the internal identity hash in the database schema. The primary must be
cryptographically sound (ruling out MD5 and SHA1). If SHA256 is competitive with
or faster than BLAKE3 on Apple ARM, prefer SHA256 for its universal forensic
utility.

## Processing Order

For each file entering `Processor::process()`:

1. Read file content, compute **phase-1 hashes** (primary + hashset-required
   algorithms, single I/O pass)
2. **Dedup check** — look up primary hash in shared dedup set. If already seen,
   skip all content processing. If new, insert it.
3. **Inclusion check** — look up hash in inclusion hashset. If hit, write record
   to `hashset_hits` table. Proceed with full processing.
4. **Exclusion check** — look up hash in exclusion hashset. If hit, write primary
   hash to hash table, skip remaining content processing.
5. **Phase-2 hashes** — seek back to start, compute remaining trifecta algorithms
   (MD5/SHA1/SHA256 minus whatever phase 1 already covered) in a second I/O pass.
6. Proceed with signatures, grep, rule evaluation.

**Inclusion beats exclusion** on conflict — if a file's hash appears in both
sets, it is treated as an inclusion hit (full processing + hashset_hits record).

## Two-Phase Hashing

### Phase 1 (every file)

Computed in a single I/O pass:

- The primary hash algorithm (SHA256 or BLAKE3, per benchmark)
- Whatever algorithm(s) the inclusion hashset requires, if different from primary
- Whatever algorithm(s) the exclusion hashset requires, if not already covered

Algorithm selection minimizes the phase-1 set by finding overlap. Example: if
inclusion hashset has {MD5, SHA1} and exclusion hashset has {SHA1, SHA256}, pick
SHA1 to satisfy both — phase 1 becomes {primary, SHA1} rather than
{primary, MD5, SHA1}.

Hashset files (libhasher hset format) can contain multiple algorithms. All
algorithm sets within an hset file cover the same files, so any supported
algorithm can be used for lookups.

### Phase 2 (non-excluded, non-deduplicated files only)

Computed in a second I/O pass (seek back via ReadSeek):

- Remaining trifecta algorithms (MD5/SHA1/SHA256) not covered by phase 1

If phase 1 happens to cover the full trifecta, no second pass is needed.

### Algorithm selection happens at initialization

The phase-1 and phase-2 algorithm sets are determined once during
`ProcessorContext` construction, not per-file. They depend only on the primary
algorithm choice and the algorithms available in the configured hashsets.

## Deduplication

An `ankerl::unordered_dense::set<std::string>` in `ProcessorContext` holds the
primary hash of every file whose primary hash has been computed. This includes
files that were excluded, included, or fully processed — any file that has been
"handled" from a content perspective.

A duplicate file's dirent/inode metadata is still recorded (that happens upstream
in TskReader, before Processor). Only content-based processing is skipped: hash
table writes, file signatures, grep/search hits, rule evaluation.

### Thread safety

Processing is multithreaded. The dedup set + a `std::mutex` are shared state in
`ProcessorContext`. The critical section is just a flat hash map lookup + insert
(~10ns), while threads spend the vast majority of time in I/O and hash
computation. A simple mutex is sufficient — reader/writer lock doesn't help
because every access is a read-then-write.

## Inclusion and Exclusion Behavior

### Exclusion

When a file's hash is found in the exclusion hashset:

- Write the primary hash to the hash table (audit trail)
- Skip all remaining content processing (no phase-2 hashes, no signatures, no
  grep, no rule evaluation)

### Inclusion

When a file's hash is found in the inclusion hashset:

- Write a record to the new `hashset_hits` table
- Proceed with full content processing (phase-2 hashes, signatures, grep, rules)

Inclusion is for bulk triage of known-bad files (e.g., VirusShare hash sets),
not rule-based identification of specific threats.

## Database: hashset_hits Table

New table: `hashset_hits`

| Column      | Type    | Description                          |
|-------------|---------|--------------------------------------|
| Hash        | VARCHAR | Primary hash of the matching file    |
| HashsetName | VARCHAR | Name from the hashset file metadata  |

Keyed by the primary hash value. Paired with the hashset's name so that when
multiple inclusion sets are used, the examiner knows which set matched.

## Architecture

### Helper class (per-Processor, name TBD)

Each Processor constructs its own instance of a helper class that encapsulates
the hash computation and content triage logic. Responsibilities:

- Owns its own phase-1 and phase-2 libhasher hasher objects (avoids expensive
  per-file reallocation; two hashers avoids expensive reconfiguration)
- Initialized with references to:
  - Shared dedup set + mutex (owned by `ProcessorContext`)
  - Inclusion hashset (immutable, owned by `ProcessorContext`)
  - Exclusion hashset (immutable, owned by `ProcessorContext`)
- Exposes per-file logic: compute phase-1 hashes, check dedup/inclusion/exclusion,
  compute phase-2 if needed
- Returns a disposition: deduplicated, excluded, or proceed with full processing

### ProcessorContext changes

- Owns the shared `ankerl::unordered_dense::set<std::string>` + `std::mutex`
- Owns the inclusion/exclusion `LlamaHashset` objects (already does this)
- Determines phase-1 and phase-2 algorithm sets at construction

### Processor changes

- Constructs its own helper class instance during Processor construction, using
  the shared state from ProcessorContext
- The prototype Processor (constructed in llama.cpp) creates a prototype helper;
  cloned Processors create their own helper instances with their own hasher pairs,
  all sharing the same dedup set
- `Processor::process()` delegates to the helper for hash computation and
  disposition, then acts on the result

## Testing

### Microbenchmark (test/benchmarks/)

BLAKE3 vs SHA256 vs MD5 vs SHA1, file sizes 4KB / 64KB / 512KB / 1MB / 8MB,
via libhasher. Run first to inform primary hash choice.

### Unit tests

- **Dedup:** Process two entries with identical content, verify content-based
  records are written only once
- **Exclusion:** Process a file in the exclusion set, verify only primary hash
  recorded, no signatures/grep/rules
- **Inclusion:** Process a file in the inclusion set, verify `hashset_hits`
  record written, full processing occurs
- **Inclusion beats exclusion:** File hash in both sets, verify inclusion behavior
- **Algorithm selection:** Verify phase-1 and phase-2 algorithm sets are computed
  correctly for various hashset configurations (same algorithm, different
  algorithms, multi-algorithm hset files, no hashsets provided)
- **Thread safety:** Multiple threads hitting dedup check concurrently
