# Multi-Evidence File Processing

Process multiple evidence files (disk images, directories) in a single llama invocation, producing a unified analysis database for a single forensic case.

## Motivation

A single forensic case often involves multiple evidence sources (laptop drive, external USB, phone image). Currently llama processes one evidence file per invocation. Examiners want a single unified database for cross-source analysis, with deduplication via the merkleized schema.

## CLI

```
llama [OPTIONS] OUTPUT_DIRECTORY INPUT_FILE [INPUT_FILE ...]
```

- `Options::Input` (single string) becomes `Options::Inputs` (vector of strings).
- Positional registration stays `PosOpts.add("input", -1)` — boost already collects all remaining positional args.
- `po::value<std::string>` becomes `po::value<std::vector<std::string>>` for the `"input"` option.
- `figureOutCommand()` checks `Inputs.empty()` instead of `optsMap.count("input") == 0`.
- `validateOpts()` extracts the basename from each input path and rejects duplicates at startup (duplicate filenames are an error because the filename is the evidence file's identity in the database).
- Help text updated to show `INPUT_FILE [INPUT_FILE ...]`.

## Database Schema

### New Tables

#### `evidence_files`

| Column | Type | Key | Description |
|--------|------|-----|-------------|
| Name | VARCHAR | PK | Evidence filename (basename, e.g., "laptop.E01") |
| Path | VARCHAR | | Full filesystem path for operational use |
| ImageType | VARCHAR | | TSK image type name ("raw", "ewf") or "directory" |
| ImageDescription | VARCHAR | | TSK image type description |
| ImageSize | BIGINT | | Total image size in bytes |
| SectorSize | INTEGER | | Sector size |
| VerificationHash | VARCHAR | | Embedded integrity hash (E01 etc.), null if unavailable |

#### `volumes`

| Column | Type | Key | Description |
|--------|------|-----|-------------|
| EvidenceFileName | VARCHAR | PK, FK → evidence_files.Name | Parent evidence file |
| Addr | INTEGER | PK | Partition address |
| TableNum | INTEGER | PK | Table number |
| Description | VARCHAR | | Partition description |
| Flags | VARCHAR | | Alloc/Unalloc/Meta |
| NumBlocks | BIGINT | | Number of blocks in partition |
| SlotNum | INTEGER | | Slot number |
| StartBlock | BIGINT | | Starting block |
| VsType | VARCHAR | | Volume system type (MBR, GPT, etc.) |
| VsDescription | VARCHAR | | Volume system description |
| VsBlockSize | INTEGER | | Volume system block size |

#### `filesystems`

| Column | Type | Key | Description |
|--------|------|-----|-------------|
| EvidenceFileName | VARCHAR | PK, FK → evidence_files.Name | Parent evidence file |
| ByteOffset | BIGINT | PK | FS byte offset within image (0 for posix) |
| VolumeAddr | INTEGER | FK → volumes.Addr | Parent volume (null for bare FS or posix) |
| VolumeTableNum | INTEGER | FK → volumes.TableNum | Parent volume (null for bare FS or posix) |
| Type | VARCHAR | | FS type name (ntfs, ext4, hfs, "posix") |
| BlockSize | INTEGER | | Block size |
| BlockCount | BIGINT | | Total blocks |
| DeviceBlockSize | INTEGER | | Device block size |
| BlockName | VARCHAR | | Block name (e.g., "Cluster" for NTFS) |
| LittleEndian | BOOLEAN | | Endianness |
| FirstBlock | BIGINT | | First block number |
| FirstInum | BIGINT | | First inode number |
| LastBlock | BIGINT | | Last block number |
| LastInum | BIGINT | | Last inode number |
| Flags | VARCHAR | | FS flags (sequenced, nanosecond, etc.) |
| FsID | VARCHAR | | Filesystem UUID |
| JournalInum | BIGINT | | Journal inode number |
| RootInum | BIGINT | | Root inode number |
| NumInums | BIGINT | | Total inode count |
| RootDirentId | VARCHAR | | ID of the root Dirent record for this filesystem |

### Changes to Existing Tables

**`inode`**: Add `EvidenceFileName VARCHAR` and `ByteOffset BIGINT` as a composite FK → filesystems(EvidenceFileName, ByteOffset). This replaces the existing `FsOffset` column (same semantic value, aligned naming).

**`dirent`**: No changes. Dirents are content-addressed via RecordHasher. Keeping them free of filesystem/evidence references enables deduplication of identical directory structures across filesystems and evidence files through merkleization.

**`hash`**: No changes. Hashes are universal — if two files on different filesystems share the same SHA256, they have the same content, search hits, and rule matches. Filesystem context is reachable via inode.

**`exception_log`**: Already has `evidence_file` and `fs_offset` columns. Align naming if needed but no structural changes.

## TskImgAssembler Refactoring

TskImgAssembler currently builds a JSON document via its state machine (INIT → IMG → VS → VOL → VOL_FS → END) but the output is never consumed. Refactor to emit database records.

### Changes

- Replace `jsoncons::json Doc` with structured record state: an `EvidenceFileRec`, vector of `VolumeRec`, vector of `FilesystemRec`.
- `addImage()` populates `EvidenceFileRec` from TSK_IMG_INFO fields (same conversion logic as tskconversion.cpp, but to struct fields instead of JSON).
- `addVolumeSystem()` stores VS metadata (type, description, blockSize) to stamp onto subsequent volume records.
- `addVolume()` creates a `VolumeRec` with the current VS context.
- `addFileSystem()` creates a `FilesystemRec`, linking to the current volume if applicable.
- `dump()` replaced with accessors or a flush method that writes all records to DuckDB via appenders.

### What stays

The state machine stays — it correctly validates TSK callback ordering (image before volume system, volume system before volume, etc.).

### New record structs

- `EvidenceFileRec` — matches evidence_files table columns
- `VolumeRec` — matches volumes table columns
- `FilesystemRec` — matches filesystems table columns

### TskReader integration

- After `startReading()` completes the walk, flush the assembler's records to DB.
- The assembler tracks the current filesystem's `EvidenceFileName` + `ByteOffset` so TskReader can stamp them onto Entry and Inode during `addToBatch()`.
- `FsIndex` tracking moves into the assembler (it already knows when a new FS starts).

### Entry struct changes

- `Entry::EvidenceFile` (currently the full path) stays for operational use (Processor needs it to open TSK images). No rename needed — it's not a DB column.
- `Entry::FsOffset` stays as-is — used by Processor's FsHandles cache for TSK_FS_INFO lookups.
- Inode population in `addToBatch()` changes: set `inode.EvidenceFileName` (basename) and `inode.ByteOffset` instead of `inode.FsOffset`.

## Processing Loop

### Architecture: Outer Loop with One-Ahead Prefetch

Sequential processing with input prefetching for fast startup.

```
init():
  // parallel futures (same as today, plus first input)
  future: openInput(Inputs[0])
  future: dbInit()          // now creates 3 new tables too
  future: loadRules()
  future: loadPatterns()
  future: loadSignatures()
  wait all

search():
  init()
  create shared resources: LgProg, ProcessorContext, ProgressInfo

  for i in 0..Inputs.size():
    // Input[i] is already open (from init or previous iteration's prefetch)

    // prefetch: start opening next input while processing current
    if (i + 1 < Inputs.size()):
      nextInput = make_future(Pool, openInput(Inputs[i+1]))

    // write evidence_files record to DB

    create FileScheduler       // fresh per evidence file
    create BatchHandler
    wire Input -> BatchHandler -> FileScheduler

    progressInfo.setEvidenceFile(filename, index, total)

    Input->startReading()      // walk + process
    scheduler->waitForCompletion()  // wait for all batches to finish

    // assembler flushes volume/filesystem records to DB

    // ensure next input is ready before next iteration
    if nextInput:
      nextInput.get()

  progressInfo.setDone()
  write reports
  export DB
```

**Thread pool completion**: `boost::asio::thread_pool::join()` terminates threads and cannot be reused. Since the pool is shared across evidence files, we need a different mechanism. FileScheduler (or a wrapper) tracks outstanding batch count via an atomic counter and a promise/future or condition variable. When the last batch completes, it signals done. The main loop calls `scheduler->waitForCompletion()` instead of `Pool.join()`. The final `Pool.join()` happens once at shutdown after all evidence files are processed.

### Resource Lifecycle

| Resource | Lifetime | Rationale |
|----------|----------|-----------|
| Thread pool (Pool) | Once, shared; join() at shutdown only | Avoid teardown/setup overhead |
| ProcessorContext | Once, shared | Rules, hashsets, signatures, LgProg don't change |
| Processor prototype | Once, shared | Cloned per task in pool, references shared context |
| FileScheduler | Per evidence file | Bucket state is per-image (disk-locality offsets meaningless across images) |
| BatchHandler | Per evidence file | Wired to its FileScheduler |
| InputReader | Per evidence file | TskReader or PosixReader, one-ahead prefetch |
| TskImgAssembler | Per evidence file | Tracks image/volume/filesystem hierarchy for current input |
| ProgressInfo | Once, shared | Resets counters per evidence file, tracks overall position |

## Progress Reporting

### Changes to ProgressInfo

- Add `EvidenceFileName`, `EvidenceIndex`, `EvidenceCount` fields.
- New method: `setEvidenceFile(name, index, total)` — called before each evidence file's walk, resets filesystem counters.
- `setFilesystem()` stays as-is — called per filesystem within an evidence file.

### Display format

Multiple inputs:
```
[2/3] usb.dd | FS 2 | 1,234 inodes | 456 MB | 12.3 MB/s
```

Single input (suppress evidence prefix — no noise):
```
FS 2 | 1,234 inodes | 456 MB | 12.3 MB/s
```

## PosixReader / DirectoryReader Support

Low priority — design for it in the schema, stub or defer implementation.

When processing a directory input:

- `evidence_files` record: Name = directory basename, ImageType = "directory", ImageSize/SectorSize = null, VerificationHash = null.
- Synthetic `filesystems` record: ByteOffset = 0, Type = "posix", volume FK = null, metadata fields = null, RootDirentId = root dirent of the directory walk.
- No `volumes` record.
- PosixReader needs evidence file context passed in at construction so it can stamp `EvidenceFileName` and `FsByteOffset = 0` onto Inode and Entry records.

## Report SQL Changes

All three report queries (file_inventory.sql, rule_hits_report.sql, search_hits_report.sql) are updated to:

- Join through `inode` → `filesystems` → `evidence_files` to obtain evidence file context.
- Add `EvidenceFileName` as a leading column in SELECT.
- Add to ORDER BY so results group by evidence file.

## Scope Boundaries

**In scope:**
- Multiple positional evidence file inputs via CLI
- New schema tables (evidence_files, volumes, filesystems)
- TskImgAssembler refactoring to relational records
- Sequential outer loop with one-ahead prefetch
- Progress bar updates for multi-evidence context
- Report SQL updates
- Duplicate filename validation

**Out of scope / deferred:**
- Case manifest file (`--case-file`)
- Concurrent evidence processing
- PosixReader/DirectoryReader implementation (schema designed for it, implementation stubbed)
- Evidence file integrity hash verification (just store what's embedded)
