# Inode Id as Cross-Image Primary Key

**Date:** 2026-05-23
**Branch:** readseek-contract-hardening (or successor)
**Status:** Design — approved for plan-writing

## Problem

Llama's parquet output collapses inodes from multiple evidence files into rows that share `MetaAddr` values without a per-row disambiguator. On a two-image corpus (the szechuan E01s: 2 evidence files, 5 filesystems, 188,624 inodes, 340,212 dirents):

- 84,146 of 104,260 distinct `MetaAddr` values (~81%) appear in more than one filesystem.
- Joining `dirent` to `inode` on `MetaAddr` alone is unambiguous for only 32,073 of 340,212 dirent rows (~9.4%). The other 90.6% are ambiguous because `dirent` carries no evidence-file or filesystem-offset column at all.
- `hash.MetaAddr` and `extents.Inode` have the same problem.
- `inode.Id` exists in the schema but is never populated.

Any analyst query that joins these tables in a multi-image run is silently wrong.

## Goal

Populate `inode.Id` as a stable, cross-image-unique identifier, and reference it from every fact table that today refers to an inode by raw `MetaAddr`. The Id is a hex-encoded TreeHasher digest of `(EvidenceFileName, FsByteOffset, Addr, SeqNum)` — the minimal tuple that uniquely identifies an inode incarnation across a multi-image corpus.

`SeqNum` is included for forensic correctness: it lets recovered/orphan dirents whose MFT sequence has been reused correctly fail to resolve against the live inode (rather than silently joining to the wrong incarnation). In the szechuan corpus, 6 dirents exhibit this stale-seq pattern; today they appear to point at unrelated live inodes.

## Schema changes

### `inode`

No structural change. The existing `Id` column starts being populated as the 64-char hex of `hashInode(inode)`. `EvidenceFileName`, `ByteOffset`, `Addr`, `SeqNum` remain — they are the hash inputs, kept for human readability and as a fallback join path.

### `dirent`

| Op | Column | Type | Notes |
|---|---|---|---|
| add | `MetaId` | VARCHAR | Strict FK to `inode.Id`; hashed from `(EvidenceFileName, FsByteOffset, MetaAddr, MetaSeq)`. The value is always populated; an orphan dirent whose `MetaSeq` differs from the live inode's `SeqNum` will produce a `MetaId` that doesn't match any row in the inode table, so a LEFT JOIN yields NULL on the inode side. |
| add | `ParentId` | VARCHAR | Strict FK to the parent directory's `inode.Id`; hashed from `(EvidenceFileName, FsByteOffset, ParentAddr, ParentSeq)`. |

`Id`, `Path`, `Name`, `ShortName`, `Type`, `Flags`, `MetaAddr`, `ParentAddr`, `MetaSeq`, `ParentSeq` are unchanged. The existing `dirent.Id` (a hash over path/name/addr/seq via `RecordHasher::hashDirent`) is left alone — it is a dirent-identity hash distinct from the new inode-reference fields. Revisiting whether `dirent.Id` itself should incorporate evidence-file context is out of scope.

### `hash`

| Op | Column | Type | Notes |
|---|---|---|---|
| add | `InodeId` | VARCHAR | FK to `inode.Id`. |
| drop | `MetaAddr` | UBIGINT | Useless without evidence-file context. |

`MD5`, `SHA1`, `SHA256`, `Blake3`, `Ssdeep` unchanged.

### `extents`

| Op | Column | Type | Notes |
|---|---|---|---|
| add | `InodeId` | VARCHAR | FK to `inode.Id`. |
| drop | `Inode` | UBIGINT | Useless without evidence-file context. |
| drop | `FilesystemOffset` | UBIGINT | Was the only fs-disambiguator; now encoded in `InodeId`. |

`PhysicalStart/End`, `LogicalStart/End`, `Path`, `Flags`, `Source` unchanged.

### `filesystems`

| Op | Column | Type | Notes |
|---|---|---|---|
| add | `RootInodeId` | VARCHAR | Hash of `(EvidenceFileName, ByteOffset, RootInum, 0)`. |

`RootDirentId` is retained (always empty today) — conservative, to avoid breaking any external consumer that reads the column.

### Plugin tables

`lnk_files`, `jumplist_auto_entries`, `jumplist_custom_entries`, `evtx_event`, `evtx_file` are unchanged. They are keyed by the source file's `sha256`; the join from sha256 → inode goes through the `hash` table, whose new `InodeId` does the disambiguation.

## Hashing scheme

Reuse the existing `RecordHasher` / `TreeHasher` / `FieldHash` infrastructure that already produces `dirent.Id`. Output is 32 bytes, hex-encoded to a 64-character string.

A new shared helper is the canonical site for the four-input hash. Both `RecordHasher::hashInode(const Inode&)` and the dirent-FK computation in `DirentStack::pop` call into it so the two sides cannot drift:

```cpp
FieldHash RecordHasher::hashInodeIdentity(
    std::string_view evidenceFileName,
    uint64_t fsByteOffset,
    uint64_t addr,
    uint64_t seqNum);
```

Hash inputs, in this order:

1. `EvidenceFileName` (string)
2. `FsByteOffset` (uint64) — the filesystem's byte offset within the image
3. `Addr` (uint64) — the inode number
4. `SeqNum` (uint64) — MFT sequence number (zero for FAT; populated for NTFS)

**Not hashed:** type, flags, filesize, uid/gid, timestamps, link target, num links, metadata blob, attr/stream sub-hashes. Reasoning: the Id is an *identity* hash, not a *content* hash. It should be stable across re-imaging variations (clock skew, attr re-parse) and stable when Inode gains fields later. The four inputs above are exactly what makes an inode unique in a multi-image corpus.

Determinism guarantees that `dirent.MetaId` computed from `(EvidenceFileName, FsByteOffset, MetaAddr, MetaSeq)` matches the inode-side hash byte-for-byte when `MetaSeq == SeqNum`.

## Code-flow changes

Approach A: precompute on the Inode, plumb the string downstream to consumers; reconstruct from dirent context on the dirent side. Hash is computed at most twice per inode-FK relationship: once on the inode (canonical), once on each referrer that doesn't already have the value in hand.

### `include/inode.h`

No struct change. `Id` field already exists.

### `include/direntbatch.h`

Widen `Dirent`:

```cpp
std::string EvidenceFileName;   // new — in-struct only, NOT emitted to parquet
uint64_t    FsByteOffset;       // new — in-struct only, NOT emitted to parquet
// ...existing fields unchanged...
std::string MetaId;             // new — emitted to parquet
std::string ParentId;           // new — emitted to parquet
```

`ColNames` adds `MetaId` and `ParentId`. `EvidenceFileName` and `FsByteOffset` are deliberately excluded from `ColNames` — they are carried in-struct purely so `DirentStack::pop` can compute the FKs.

### `include/duckhash.h` (HashRec)

```cpp
struct HashRec {
  void set(SFHASH_HashValues h, std::string inodeId, uint64_t hashAlgs);
  static constexpr auto ColNames = {"InodeId", "MD5", "SHA1", "SHA256", "Blake3", "Ssdeep"};
  std::string InodeId;          // replaces MetaAddr
  // hash fields unchanged
};
```

### `include/entry.h`

Entry already carries `EvidenceFile`, `FsOffset`, `Addr`. Add:

```cpp
std::string InodeId;            // precomputed by TskReader, consumed by Processor
```

### `include/recordhasher.h`

```cpp
FieldHash hashInode(const Inode& r);
```

(plus the shared `hashInodeIdentity` helper described above, exposed for use by `DirentStack`.)

### `src/recordhasher.cpp`

```cpp
FieldHash RecordHasher::hashInode(const Inode& r) {
  return hashInodeIdentity(r.EvidenceFileName, r.ByteOffset, r.Addr, r.SeqNum);
}
```

`hashDirent(const Dirent& r)` is left alone. The new MetaId/ParentId are computed via `hashInodeIdentity` directly from `DirentStack::pop`, not via a modified `hashDirent`.

### `src/tskreader.cpp` — `addToBatch`

After `inode.EvidenceFileName = ...; inode.ByteOffset = CurFsOffset;`:

```cpp
inode.Id = hexEncode(RecHasher.hashInode(inode).hash, 32);
// ...
entry->InodeId = inode.Id;
```

Where the dirent is built (around lines 178–188), before `Dirents.push(std::move(dirent))`:

```cpp
dirent.EvidenceFileName = std::filesystem::path(ImgPath).filename().string();
dirent.FsByteOffset     = CurFsOffset;
```

### `src/direntstack.cpp` — `pop()`

After the existing `hashDirent`/`rec.Id` assignment:

```cpp
rec.MetaId   = hexEncode(
  RecHasher.hashInodeIdentity(rec.EvidenceFileName, rec.FsByteOffset,
                              rec.MetaAddr, rec.MetaSeq).hash, 32);
rec.ParentId = hexEncode(
  RecHasher.hashInodeIdentity(rec.EvidenceFileName, rec.FsByteOffset,
                              rec.ParentAddr, rec.ParentSeq).hash, 32);
```

### `src/processor.cpp` — `process()`

Change `HashRecord.set(h, entry.Addr, HashAlgs)` to `HashRecord.set(h, entry.InodeId, HashAlgs)`. No other change. `FileSigs` and `SearchHits` continue to key off sha256.

### `src/tskimgassembler.cpp`

In `addFileSystem`, compute and store `RootInodeId` from `(evidenceFileName, byteOffset, root_inum, root_seq)`:

```cpp
fs.RootInodeId = hexEncode(
  RecHasher.hashInodeIdentity(evidenceFileName, byteOffset, root_inum, root_seq).hash, 32);
```

(The assembler doesn't currently hold a `RecordHasher`; one will need to be passed in, or the helper exposed as a free function.)

Implementation note: `root_seq` must come from the same TSK source that populates the inode-side `SeqNum`, so that the FK matches the row in the inode table. For NTFS the root sequence is conventionally non-zero (the `$Root` MFT entry); for FAT it is 0. The implementer should source `root_seq` from the FS-level TSK metadata at `filterFs` time, or — if that value isn't available there — defer setting `RootInodeId` until the root inode itself has been emitted and copy its `Id` directly.

### `src/posixreader.cpp`

Leave `inode.Id = ""` as-is. Add a `// TODO` comment referencing this design doc — PosixReader is suspected broken and will be revisited in a separate effort.

### `include/evidencerec.h`

`FilesystemRec`: add `RootInodeId` to the struct and to `ColNames`. Keep `RootDirentId`.

### Extents writer

The file that writes the `extents` table was not located during design. Locating it and applying the same `InodeId` plumbing (drop `Inode` + `FilesystemOffset`, add `InodeId` computed via the shared helper) is part of the implementation plan.

## Testing

### Unit tests

- `test/test_recordhasher.cpp` — verify `hashInode(Inode{efn, offset, addr, seq})` produces known hex values for two fixed-vector inputs: one NTFS-style (seq nonzero), one FAT-style (seq=0).
- Same file — verify `hashInode(const Inode&)` and `hashInodeIdentity(...)` called with the same four inputs produce byte-identical output (the shared-helper invariant).
- `test/test_direntstack.cpp` — extend existing tests so the input dirents set `EvidenceFileName`/`FsByteOffset`, and assert that popped records have non-empty, deterministic `MetaId` and `ParentId`.
- `test/test_tskimgassembler.cpp` — assert `RootInodeId` is populated and matches the hash of the expected `(efn, byteOffset, root_inum, root_seq)`.

### Out of scope for testing

- **Integration test against the szechuan corpus** — not included. Implementer can spot-check manually if desired.
- **PosixReader** — no tests; Id stays empty by design.
- **Backwards compat** — parquet schema is regenerated per run; `load.sql` and `schema.sql` are produced from the same writer code path, so they're always in sync with what the writer emits.
- **Hash stability across llama versions** — not asserted. If we ever want stable Ids across versions for diffing runs, that's a separate design.

## Non-goals

- Changing `dirent.Id` semantics. (Possible future work.)
- Adding evidence-file context to plugin tables. (They are sha256-keyed by design.)
- Fixing the empty `RootDirentId` field. (Kept as-is.)
- Fixing PosixReader. (Tracked separately.)
- Cross-version Id stability for run-to-run diffing. (Separate design if ever needed.)
