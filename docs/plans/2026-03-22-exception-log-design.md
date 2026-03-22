# Evidence Exception Handling Design

**Goal:** Handle unreadable evidence files gracefully — log them, show them in progress, continue processing. Distinguish evidence I/O exceptions (normal forensic conditions) from application errors (bugs).

**Context:** The ReadSeek contract hardening work changed ReadSeekTSK to throw on `tsk_fs_file_read` errors instead of silently returning 0. This correctly surfaces problems like NTFS compressed files that libtsk can't decompress (e.g., deleted compressed files with corrupt compression units). These are not bugs — they're the state of the evidence. But the exception currently propagates uncaught and crashes the program.

---

## Design

### 1. EvidenceIOError exception type

A new exception class inheriting `std::runtime_error`:

```cpp
class EvidenceIOError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};
```

Thrown by ReadSeekTSK and ReadSeekFile when evidence I/O fails. Replaces the generic `std::runtime_error` thrown by `THROW_IF` in those implementations. ReadSeekBuf is in-memory and effectively never throws.

This lets Processor catch `EvidenceIOError` specifically and continue, while application errors (`std::runtime_error` from lightgrep, hashing libraries, etc.) propagate and crash — those are genuine bugs.

### 2. Entry enrichment

Entry gains fields for full evidence context, populated by TskReader at construction:

| Field | Type | Description |
|-------|------|-------------|
| Addr | uint64_t | Metadata address (existing field) |
| EvidenceFile | std::string | Image path, e.g., "donald_blake.E01" |
| FsIndex | uint32_t | Filesystem number within the image |
| FsOffset | uint64_t | Byte offset of the filesystem |
| AddrFlags | uint32_t | Raw TSK inode flags (TSK_FS_META_FLAG_ENUM) |
| Path | std::string | File path from TSK walk callback |
| FileSize | uint64_t | File size from meta->size |

TskReader has all this information at the `addToBatch` call site. The `path` argument currently ignored in `processFile` will be threaded through. `Addr` is kept as the field name because it generalizes beyond Unix inodes to MFT record IDs.

### 3. Exception log table

New DuckDB table created at startup alongside existing tables:

```sql
CREATE TABLE exception_log (
  evidence_file VARCHAR NOT NULL,
  fs_index      UINTEGER,
  fs_offset     UBIGINT,
  addr          UBIGINT,
  addr_flags    VARCHAR,
  path          VARCHAR,
  file_size     UBIGINT,
  operation     VARCHAR,
  timestamp     TIMESTAMP NOT NULL,
  message       VARCHAR NOT NULL
)
```

Nullable fields support exceptions at any level of the evidence hierarchy:
- File-level: all fields populated
- Filesystem-level: addr through file_size NULL
- Image-level: only evidence_file, timestamp, message

The `addr_flags` column stores a human-readable string (e.g., "Deleted, Compressed") converted from the raw TSK flags at write time. The `operation` column is a free-form string identifying what was being attempted: "hash", "signature", "search", "pdf_extract", "open".

Writing follows the existing batch appender pattern — an `ExceptionAppender` accumulates records and flushes with the rest of the batch in `Processor::flush()`.

### 4. Exception handling in Processor::process()

Each operation in `process()` gets its own try/catch for `EvidenceIOError`:

- **Hashing fails** → log exception, early return (no blake3 hash means we can't write hash records or do signature/search)
- **Signature detection fails** → log exception, continue to search
- **Search fails** → log exception, continue
- **PDF extraction fails** → log exception, fall back to raw search

Any non-`EvidenceIOError` exception propagates uncaught — those are application bugs.

### 5. Progress bar exception count

ProgressInfo gains an atomic exception counter. Processor increments it when catching an `EvidenceIOError`. The progress bar display appends `| N exceptions` when the count is nonzero. No clutter when things are clean.

### 6. End-of-run summary

After processing completes, if the exception count is nonzero, print a summary line:

```
N evidence I/O exceptions encountered — see exception_log table
```
