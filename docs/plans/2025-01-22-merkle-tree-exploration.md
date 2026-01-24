# Merkle Tree Data Model Exploration

**Date:** 2025-01-22
**Context:** Post-DFIR Bayou exploratory discussion
**Status:** Exploratory - not yet implemented

---

## Background

Following the successful DFIR Bayou talk (which demonstrated FIEMAP-based filesystem processing), this discussion explored using Merkle trees to enable fleet-scale forensic monitoring and efficient cross-system, cross-time comparison.

The motivating scenario: running llama daily on 5000 servers, with storage that doesn't grow linearly, and the ability to detect anomalies across the fleet.

---

## Current Llama Data Model

### Existing Tables

**Dirent** (directory entry):
- Id (hash of record via FieldHasher)
- Path, Name, ShortName
- Type, Flags
- MetaAddr → joins to Inode.Addr
- ParentAddr, MetaSeq, ParentSeq

**Inode**:
- Id (hash of record via FieldHasher)
- Type, Flags
- Addr, FsOffset, Filesize
- Uid, Gid, NumLinks, SeqNum
- Created, Accessed, Modified, Metadata
- LinkTarget

**HashRec** (content hashes):
- MetaAddr → joins to Inode.Addr
- MD5, SHA1, SHA256, Blake3, Ssdeep

### Anticipated Addition

**Attributes** table (for NTFS ADS, extended attributes, etc.):
- Inode → Attribute (one-to-many)
- Attribute → HashRec (each attribute has its own content hash)

### Relationships

```
Current:   Dirent ──→ Inode ──→ HashRec

Planned:   Dirent ──→ Inode ──→ Attribute ──→ HashRec
```

---

## Rule Evaluation Architecture

Llama rules have sections ordered by evaluation cost (cheapest to most expensive):

| Section | Cost | Operates On | I/O Required |
|---------|------|-------------|--------------|
| `file_metadata:` | Cheapest | Dirent + Inode join | None - already in memory |
| `signature:` | Cheap | First few bytes | Minimal - read file header |
| `hash:` | Expensive | Full file content | Full file read |
| `grep:` | Most expensive | Full file content | Full file read + regex |

Sections are implicitly AND'd, enabling progressive filtering. For live triage during an active attack, filtering 95% of files by metadata alone avoids opening those file handles entirely.

This architecture suggests that caching/hashing should align with these sections - different granularity for different rule types.

---

## The Two-Tree Model

### Problem Statement

For fleet monitoring, we need:
1. **Deduplication** - Same file across 5000 servers stored once
2. **Change detection** - Notice when any forensically-relevant field changes
3. **Efficient comparison** - Diff two snapshots in O(changes) not O(total files)

These goals conflict: aggressive deduplication (ignore timestamp changes) loses change detection; full state tracking (include all fields) reduces deduplication.

### Solution: Identity Tree + Forensic Graph

**Identity Tree** - Merkle tree on `(path, type, size, content_hash)`:
- Answers: "Is this the same file (content at location)?"
- Stable across metadata churn (timestamp changes don't affect it)
- Enables massive deduplication across fleet and time
- `/usr/bin/ls` with same content → same identity across 5000 servers

**Forensic Graph** - Full state from `(Dirent.Id, Inode.Id, Attribute.Ids)`:
- Answers: "Has anything changed about this file?"
- Captures every forensic detail including timestamps
- Enables full audit trail

### Relationship Between Trees

Same identity can have multiple states:

```
Identity: /etc/passwd (abc123)
    ├── State A: {dirent.Id=x, inode.Id=y} @ server1, day1
    ├── State A: {dirent.Id=x, inode.Id=y} @ server2, day1  (same state)
    ├── State B: {dirent.Id=x, inode.Id=z} @ server3, day1  (different!)
    └── State A: {dirent.Id=x, inode.Id=y} @ server1, day2  (unchanged)
```

### Query Patterns This Enables

| Question | Approach |
|----------|----------|
| What files are identical across fleet? | Group by identity_hash |
| Which server has different /etc/passwd? | Same path, different identity |
| What metadata changed but content didn't? | Same identity, different state |
| Minimal storage for 5000 servers × 365 days? | Dedupe on identity, store unique states |
| Did permissions change over time? | Same identity, compare states |

---

## The Churn Problem

### Not All Filesystem Areas Are Equal

| Zone | Churn | Interest | Examples |
|------|-------|----------|----------|
| High churn, low interest | High | Low | /tmp, /var/cache, ~/.cache |
| High churn, high interest | High | High | Auto-update dirs, package managers |
| Low churn, high interest | Low | High | /etc, /usr/bin, /usr/lib |
| Low churn, low interest | Low | Low | Static content |

### Pragmatic Approach

Rather than designing around theoretical churn concerns:
1. Build the identity tree + forensic graph structure
2. Run on real systems, measure actual storage and processing costs
3. If churn breaks the bank, add path-based filtering at that point
4. Real data will guide which optimizations are actually needed

---

## Role of the Merkle Tree

The Merkle tree is **not** the detection mechanism - that's what rules, IOC matching, embeddings, and anomaly detection are for.

The Merkle tree is **investigation infrastructure**:

```
Detection Layer                    Investigation Layer
───────────────                    ───────────────────
IOC rules                          "Rule fired on server47,
Embeddings/similarity               what else changed?"
Anomaly detection
Behavioral patterns                "This file appeared yesterday,
        │                           does it exist elsewhere?"
        ▼
   ALERT: Investigate      ───→    Merkle tree enables efficient
                                   answers to these questions
```

Detection finds the rabbit hole. The Merkle tree lets you efficiently explore it.

---

## Tree Diffing in SQL

DuckDB supports recursive CTEs, enabling tree diff operations:

```sql
-- Simple approach: compare all nodes directly
SELECT
    COALESCE(s1.path, s2.path) as path,
    CASE
        WHEN s1.path IS NULL THEN 'added'
        WHEN s2.path IS NULL THEN 'deleted'
        WHEN s1.state_hash != s2.state_hash THEN 'modified'
    END as change_type
FROM snapshot_1 s1
FULL OUTER JOIN snapshot_2 s2 USING (path)
WHERE s1.state_hash IS DISTINCT FROM s2.state_hash;
```

More sophisticated recursive CTEs can implement the Merkle optimization (skip matching subtrees), but whether DuckDB's planner actually exploits this depends on execution. Benchmarking on real data would determine if the optimization is worth the complexity.

---

## FIEMAP/Driver Observations from Bayou Talk

- FIEMAP works for XFS filesystem processing (demo was successful)
- NTFS via Linux driver does NOT work with FIEMAP (driver-dependent)
- ntfs3 driver claims FIEMAP support but may have issues
- Need empirical testing to determine which drivers support FIEMAP

---

## Journals vs Write-Ahead Logs

Joe Sylve raised that some modern filesystems lack persistent journals:

| Filesystem | Mechanism | Forensic Value |
|------------|-----------|----------------|
| ext4 | JBD2 journal (persistent) | High |
| XFS | Intent log (persistent) | High |
| Btrfs | COW + transaction groups | Medium-High |
| ZFS | COW + uberblocks | High |
| APFS | Checkpoint system (no journal) | Low |
| F2FS | Checkpoint + NAT journal | Medium |

APFS deliberately eliminated the persistent journal that HFS+ had. For APFS forensics, alternatives include:
- APFS snapshots/Time Machine snapshots
- Container superblock archaeology
- Object map analysis
- Spaceman free space analysis
- B-tree node carving
- SQLite WAL files (app-level, not filesystem)

---

## What Llama Already Has

- `Id` fields with FieldHasher → per-record hashing ✓
- DuckDB storage with boost::pfr → efficient batch storage ✓
- Dirent → Inode → HashRec relationships ✓
- Rule engine with progressive filtering ✓
- PosixReader with FIEMAP for Linux native processing ✓

## What's Missing

- Attributes table (inode → attribute → hashrec)
- Merkle composition layer (identity tree + forensic graph)
- Investigation queries (diff, timeline, cross-system comparison)
- Fleet collection integration (Velociraptor deployment)

---

## Next Steps (When Ready to Implement)

1. Complete the Attributes table in the data model
2. Define identity_hash computation (path, type, size, content_hash)
3. Implement tree storage schema in DuckDB
4. Build basic diff queries
5. Deploy on real systems via Velociraptor during IR
6. Measure actual storage/processing costs
7. Add churn filtering if needed based on real data
8. Build investigation query layer

---

## Open Questions

1. Exact fields for identity_hash vs state_hash?
2. How to handle hard links (one inode, multiple paths)?
3. Storage schema for efficient cross-snapshot queries?
4. Should Accessed time be excluded from state_hash (churn) or included (audit)?
5. What visualization helps investigators explore tree diffs?

---

**End of Exploration Summary**
