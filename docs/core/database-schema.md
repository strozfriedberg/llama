# Database Schema

Llama exports its results to a DuckDB database (and, optionally, parquet files for offline analysis). This document describes the schema, the design rationale behind the inode identity model, and example queries.

## Rationale

A single forensic run frequently spans **multiple evidence images**, and a single image often contains **multiple filesystems**. The natural per-filesystem inode address (`MetaAddr`) is only unique within its filesystem — across two NTFS partitions, MFT entry 5 means two different files. Joining tables on those integers silently merges unrelated records when more than one filesystem is in play, which masks real findings and produces false ones.

Llama solves this by giving every inode a **32-byte binary identity hash** (`inode.Id`, stored as `BLOB`) that is globally unique across all filesystems in all images in a run. Every foreign-key column referencing an inode is a BLOB FK against `inode.Id`, not the per-filesystem integer.

## The identity hash

```
inode.Id = SHA256_field_hash(EvidenceFileName, FsByteOffset, Addr, SeqNum)
```

The result is stored as a raw 32-byte `BLOB` (not hex text). The four inputs together pin a specific inode incarnation: which evidence file it lives in, which filesystem within that image (the FS's byte offset), which inode slot (`Addr`), and which generation of that slot (`SeqNum`). The SeqNum component matters because filesystems reuse inode slots — when a file is deleted and the slot is later reallocated, the new file has the same `Addr` but a different `SeqNum`. A stale dirent pointing to the deleted file's `(Addr, oldSeqNum)` produces a different `inode.Id` than the new file's `(Addr, newSeqNum)`, so the dirent correctly resolves to nothing instead of falsely matching the new file.

The hash is computed once per inode in `TskReader::addToBatch` and is the canonical reference everywhere else.

## Tables

The inode FK chain — the core forensic relations:

| Table         | Primary key       | Notable FK columns                                |
| ------------- | ----------------- | ------------------------------------------------- |
| `inode`       | `Id`              | (PK; populated per inode walked by TSK)           |
| `dirent`      | `Id` (dirent hash)| `MetaId`, `ParentId` → `inode.Id`                 |
| `hash`        | (none; 1:1 inode) | `InodeId` → `inode.Id`                            |
| `extents`     | (none)            | `InodeId` → `inode.Id` (empty for `PosixReader`)  |
| `filesystems` | (no surrogate)    | `RootInodeId` → `inode.Id` of the FS's root inode |
| `rule_hits`   | (none)            | `inode_id` → `inode.Id`                           |

Evidence container tables (per-evidence-file metadata, not part of the inode FK chain — joined by name):

| Table             | Key            | Description                                           |
| ----------------- | -------------- | ----------------------------------------------------- |
| `evidence_files`  | `Name`         | One row per E01/raw image processed                   |
| `volumes`         | `EvidenceFileName + Addr` | Partitions found in each evidence file      |

Signature and rule reference tables — string `id` PKs are content hashes (rule body / signature definition), **not** inode identity hashes:

| Table             | Primary key | Notable FK columns                            |
| ----------------- | ----------- | --------------------------------------------- |
| `rules`           | `id`        | (rule definition)                             |
| `signatures`      | `Id`        | (signature definition: PDF, JPEG, …)          |
| `file_signatures` | (composite) | `FileHash` → `hash.SHA256`, `SigId` → `signatures.Id` |
| `search_hits`     | (none)      | `file_hash` → `hash.SHA256`, `rule_id` → `rules.id` |

Operational / diagnostic tables:

| Table          | Description                                                   |
| -------------- | ------------------------------------------------------------- |
| `exception_log`| Per-file processing errors. `addr` is per-FS uint64 — same per-filesystem-scope caveat the old `rule_hits.addr` had; do not join to `inode.Addr` across images. |
| `batches`      | Batch dispatch telemetry: `BatchId, BucketIndex, NumEntries, TotalBytes`. |
| `diskmap`      | Derived analytics table built on demand by `createDiskMap()` — not present by default. |

Plugin-defined tables: when llama loads a plugin that declares additional tables, those appear at runtime alongside the core schema. See [`plugin-development.md`](plugin-development.md) for the plugin table contract.

`MetaAddr`, `ParentAddr`, `MetaSeq`, `ParentSeq` are still emitted on `dirent` as integer columns for human readability and for filtering, but **do not use them as join keys** in multi-image or multi-filesystem queries — use the BLOB FK columns.

## Orphans and stale references

Because `MetaId` encodes `(FS, Addr, MetaSeq)`, a dirent whose target inode has been reused with an incremented `SeqNum` produces a `MetaId` that joins to no row in `inode`. A `LEFT JOIN inode ON dirent.MetaId = inode.Id` therefore yields NULL on the inode side for:

- truly orphaned dirents (the inode no longer exists), and
- dirents pointing to a stale incarnation of a now-reused inode slot.

Both are forensically interesting and the schema preserves the distinction by *not* silently joining them to the wrong inode.

## PosixReader caveat

When llama walks a live filesystem via `PosixReader` (rather than a forensic image via TSK), it does **not** currently compute `inode.Id` — the field is emitted as a zero-length BLOB, and so are `extents.InodeId`. The TODO at `src/posixreader.cpp:131` documents how to wire it up when PosixReader is revisited. Until then, treat PosixReader-produced rows as un-joinable on the new FKs.

## Working with BLOB hash columns

DuckDB BLOB columns store raw bytes. For lookups by a known hex hash,
use `unhex()`:

```sql
SELECT * FROM hash
WHERE SHA256 = unhex('e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855');
```

For human-readable output, hex-encode with `lower(hex(...))`:

```sql
SELECT lower(hex(SHA256)) AS sha256_hex FROM hash;
```

BLOB↔BLOB joins (the FK pattern across `inode.Id`, `dirent.MetaId`,
etc.) work transparently — equality is bytewise.

The following columns are `BLOB`:

| Table           | Column(s)                                 | Notes                          |
| --------------- | ----------------------------------------- | ------------------------------ |
| `inode`         | `Id`                                      | 32-byte identity hash; NOT NULL |
| `dirent`        | `Id`, `MetaId`, `ParentId`                |                                |
| `extents`       | `InodeId`                                 |                                |
| `filesystems`   | `RootInodeId`, `RootDirentId`             |                                |
| `hash`          | `InodeId`, `MD5`, `SHA1`, `SHA256`, `Blake3` | MD5/SHA1/SHA256/Blake3 nullable; InodeId NOT NULL |
| `rule_hits`     | `inode_id`                                |                                |
| `search_hits`   | `file_hash`                               |                                |
| `file_signatures` | `FileHash`                              |                                |

Columns **not** migrated to BLOB (remain VARCHAR):

| Table            | Column              | Reason                                         |
| ---------------- | ------------------- | ---------------------------------------------- |
| `hash`           | `Ssdeep`            | Fuzzy-hash text; `""` when absent              |
| `evidence_files` | `VerificationHash`  | Out of scope per spec non-goals; not FK-joined |
| `rules`          | `id`                | Content hash of rule body (string PK)          |
| `signatures`     | `Id`                | Content hash of signature definition (string PK) |

## Example queries

All examples assume the parquet files have been exported via `EXPORT DATABASE` or loaded into a DuckDB session.

**1. File inventory with hashes, one row per file**

```sql
SELECT i.EvidenceFileName, d.Path || d.Name AS FullPath,
       i.Filesize, lower(hex(h.SHA256)) AS sha256_hex
FROM dirent d
JOIN inode i ON d.MetaId = i.Id
LEFT JOIN hash h ON d.MetaId = h.InodeId
WHERE i.Type = 'File';
```

**2. Cross-image deduplication: same SHA256 in multiple images**

```sql
SELECT lower(hex(h.SHA256)) AS sha256_hex,
       COUNT(DISTINCT i.EvidenceFileName) AS image_count,
       LIST(DISTINCT i.EvidenceFileName) AS images
FROM hash h
JOIN inode i ON h.InodeId = i.Id
GROUP BY h.SHA256
HAVING image_count > 1
ORDER BY image_count DESC;
```

**3. Orphan dirents (target inode missing or stale-seq)**

```sql
SELECT d.Path, d.Name, d.MetaAddr, d.MetaSeq
FROM dirent d
LEFT JOIN inode i ON d.MetaId = i.Id
WHERE i.Id IS NULL AND d.MetaAddr <> 0;
```

**4. Find a filesystem's root inode**

```sql
SELECT fs.EvidenceFileName, fs.Type, fs.ByteOffset, i.Id AS root_inode_id
FROM filesystems fs
JOIN inode i ON fs.RootInodeId = i.Id;
```

**5. All rule hits with file context**

```sql
SELECT r.name AS rule, i.EvidenceFileName, rh.path || rh.name AS path
FROM rule_hits rh
JOIN rules r ON rh.id = r.id
LEFT JOIN inode i ON rh.inode_id = i.Id;
```

**6. All files matching a signature (e.g., every PDF across all images)**

```sql
SELECT i.EvidenceFileName, d.Path || d.Name AS FullPath,
       lower(hex(h.SHA256)) AS sha256_hex
FROM file_signatures fs
JOIN signatures s ON fs.SigId = s.Id
JOIN hash h ON fs.FileHash = h.SHA256
JOIN inode i ON h.InodeId = i.Id
LEFT JOIN dirent d ON d.MetaId = i.Id
WHERE s.Name = 'PDF';
```

**7. Hits against a specific inode (single image, single FS, single file)**

```sql
WITH target AS (
  SELECT i.Id FROM inode i
  WHERE i.EvidenceFileName = 'disk.E01'
    AND i.Addr = 12345
)
SELECT sh.pattern, sh.start_offset
FROM search_hits sh
JOIN hash h ON sh.file_hash = h.SHA256
WHERE h.InodeId IN (SELECT Id FROM target);
```
