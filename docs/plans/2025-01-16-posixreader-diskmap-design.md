# PosixReader and Disk Map Visualization Design

**Date:** 2025-01-16
**Context:** DFIR Bayou talk demo
**Status:** Approved for implementation

---

## Overview

This design covers two work streams for the DFIR Bayou demo:

1. **Docker/Build Infrastructure** - Container that builds llama + e01mount with FUSE support
2. **PosixReader Implementation** - Linux-only InputReader using FIEMAP for disk mapping
3. **Disk Map Visualization** - Norton Disk Doctor-style HTML visualization

## Demo Workflow

```
E01 in ~/ev
    ↓
e01mount (FUSE) → exposes partitions as files
    ↓
loop mount → filesystem at /mnt/fs
    ↓
PosixReader walks /mnt/fs
    ↓
FIEMAP ioctl per file → Extent records
    ↓
DuckDB Extent table (boost::pfr serialization)
    ↓
Sweep-line SQL queries → Disk map table
    ↓
HTML generator → diskmap/index.html + chunk-*.json
    ↓
python3 -m http.server → browser visualization
```

---

## Component 1: Docker Configuration

### Dockerfile Additions

```dockerfile
# Filesystem userspace tools for mounting evidence
RUN apt-get update && apt-get install -y \
    # FUSE support
    fuse3 \
    libfuse3-dev \
    # Filesystem tools
    xfsprogs \
    btrfs-progs \
    f2fs-tools \
    exfatprogs \
    ntfs-3g \
    # Loop device support
    mount \
    util-linux \
    && rm -rf /var/lib/apt/lists/*
```

### docker-build-deps.sh Additions

```bash
# Build e01 library and e01mount FUSE driver
if [ -d /code/e01 ]; then
    echo "Building e01..."
    cd /code/e01
    cargo build --release
    cargo build --package e01mount --release
else
    echo "ERROR: /code/e01 not found"
    exit 1
fi
```

### Running the Container

```bash
docker run --platform linux/arm64 -it --rm \
    --privileged \
    --device /dev/fuse \
    -v ~/code:/code \
    -v ~/ev:/ev:ro \
    llama:latest
```

---

## Component 2: Extent Struct

```cpp
// include/extent.h
#pragma once

#include <cstdint>
#include <string>

struct Extent {
  static constexpr auto ColNames = {
    "PhysicalStart",
    "PhysicalEnd",
    "LogicalStart",
    "LogicalEnd",
    "Inode",
    "FilesystemOffset",
    "Path",
    "Flags",
    "Source"
  };

  uint64_t PhysicalStart;
  uint64_t PhysicalEnd;
  uint64_t LogicalStart;
  uint64_t LogicalEnd;
  uint64_t Inode;
  uint64_t FilesystemOffset;
  std::string Path;
  std::string Flags;   // Comma-separated: "SHARED,UNWRITTEN,ENCODED"
  std::string Source;  // "filesystem" or "journal"
};
```

### DuckDB Integration

```cpp
// include/duckextent.h
#pragma once

#include "extent.h"
#include "llamaduck.h"

using ExtentBatch = DBBatch<Extent>;
```

---

## Component 3: PosixReader Class

```cpp
// include/posixreader.h
#pragma once

#ifdef __linux__

#include "inputreader.h"
#include "direntstack.h"
#include "extent.h"
#include "inode.h"
#include "recordhasher.h"

#include <sys/stat.h>
#include <filesystem>
#include <functional>
#include <vector>

class InputHandler;
class OutputHandler;
class LlamaDuck;

class PosixReader : public InputReader {
public:
  PosixReader(const std::string& mountpoint);
  virtual ~PosixReader();

  virtual void setInputHandler(const std::shared_ptr<InputHandler>& in) override;
  virtual void setOutputHandler(const std::shared_ptr<OutputHandler>& out) override;
  virtual bool startReading() override;

  // --- Testable pure functions (public for unit testing) ---

  // Convert FIEMAP flags bitmask to comma-separated string
  static std::string flagsToString(uint32_t flags);

  // Convert stat struct to Inode record
  static Inode statToInode(const struct stat& st, const std::string& path);

  // Parse raw FIEMAP output into Extent records
  static void parseExtents(
      const uint8_t* fiemapBuf,
      size_t bufLen,
      uint64_t inode,
      const std::string& path,
      uint64_t fsOffset,
      std::vector<Extent>& extentsOut
  );

  // Call FIEMAP ioctl on open fd, fills buffer (returns false on error)
  static bool callFiemap(int fd, std::vector<uint8_t>& bufOut);

private:
  void walkFilesystem();
  void handleEntry(const std::filesystem::directory_entry& entry);

  std::string Mountpoint;
  uint64_t FsOffset;

  std::shared_ptr<InputHandler> Input;
  std::shared_ptr<OutputHandler> Output;

  RecordHasher RecHasher;
  DirentStack Dirents;
};

#endif // __linux__
```

### FIEMAP Implementation

```cpp
// In posixreader.cpp
#include <linux/fiemap.h>
#include <linux/fs.h>
#include <sys/ioctl.h>

bool PosixReader::callFiemap(int fd, std::vector<uint8_t>& bufOut) {
    static constexpr size_t EXTENT_BATCH_SIZE = 1024;
    static constexpr size_t HEADER_SIZE = sizeof(fiemap);
    static constexpr size_t EXTENT_SIZE = sizeof(fiemap_extent);

    bufOut.clear();

    std::vector<uint8_t> tmpBuf(HEADER_SIZE + EXTENT_BATCH_SIZE * EXTENT_SIZE);
    uint64_t startOffset = 0;
    bool done = false;

    while (!done) {
        fiemap* fm = reinterpret_cast<fiemap*>(tmpBuf.data());
        fm->fm_start = startOffset;
        fm->fm_length = FIEMAP_MAX_OFFSET;
        fm->fm_flags = 0;
        fm->fm_mapped_extents = 0;
        fm->fm_extent_count = EXTENT_BATCH_SIZE;

        if (ioctl(fd, FS_IOC_FIEMAP, fm) == -1) {
            return false;
        }

        if (fm->fm_mapped_extents == 0) {
            break;  // No extents (empty or sparse file)
        }

        // Copy header on first iteration, extents on all iterations
        if (bufOut.empty()) {
            bufOut.insert(bufOut.end(), tmpBuf.begin(),
                          tmpBuf.begin() + HEADER_SIZE + fm->fm_mapped_extents * EXTENT_SIZE);
        } else {
            // Append just the extents
            uint8_t* extentStart = tmpBuf.data() + HEADER_SIZE;
            bufOut.insert(bufOut.end(), extentStart,
                          extentStart + fm->fm_mapped_extents * EXTENT_SIZE);
        }

        // Check if last extent has FIEMAP_EXTENT_LAST flag
        fiemap_extent* lastExtent = &fm->fm_extents[fm->fm_mapped_extents - 1];
        if (lastExtent->fe_flags & FIEMAP_EXTENT_LAST) {
            done = true;
        } else {
            // Continue from end of last extent
            startOffset = lastExtent->fe_logical + lastExtent->fe_length;
        }
    }

    // Update the header with total extent count
    if (!bufOut.empty()) {
        fiemap* finalFm = reinterpret_cast<fiemap*>(bufOut.data());
        finalFm->fm_mapped_extents = (bufOut.size() - HEADER_SIZE) / EXTENT_SIZE;
    }

    return true;
}
```

### Flag Conversion

```cpp
std::string PosixReader::flagsToString(uint32_t flags) {
    std::string result;

    if (flags & FIEMAP_EXTENT_LAST)        { result += "LAST,"; }
    if (flags & FIEMAP_EXTENT_UNKNOWN)     { result += "UNKNOWN,"; }
    if (flags & FIEMAP_EXTENT_DELALLOC)    { result += "DELALLOC,"; }
    if (flags & FIEMAP_EXTENT_ENCODED)     { result += "ENCODED,"; }
    if (flags & FIEMAP_EXTENT_DATA_ENCRYPTED) { result += "ENCRYPTED,"; }
    if (flags & FIEMAP_EXTENT_NOT_ALIGNED) { result += "NOT_ALIGNED,"; }
    if (flags & FIEMAP_EXTENT_DATA_INLINE) { result += "INLINE,"; }
    if (flags & FIEMAP_EXTENT_DATA_TAIL)   { result += "TAIL,"; }
    if (flags & FIEMAP_EXTENT_UNWRITTEN)   { result += "UNWRITTEN,"; }
    if (flags & FIEMAP_EXTENT_MERGED)      { result += "MERGED,"; }
    if (flags & FIEMAP_EXTENT_SHARED)      { result += "SHARED,"; }

    // Remove trailing comma
    if (!result.empty() && result.back() == ',') {
        result.pop_back();
    }

    return result;
}
```

---

## Component 4: Sweep-Line SQL Queries

```sql
-- Step 1: All unique boundary points
CREATE TEMP TABLE boundaries AS
SELECT DISTINCT PhysicalStart AS pos FROM extents
UNION
SELECT DISTINCT PhysicalEnd AS pos FROM extents
ORDER BY pos;

-- Step 2: Intervals between boundaries
CREATE TEMP TABLE intervals AS
SELECT
    pos AS start,
    LEAD(pos) OVER (ORDER BY pos) AS end
FROM boundaries
WHERE LEAD(pos) OVER (ORDER BY pos) IS NOT NULL;

-- Step 3: Find claimants for each interval
CREATE TABLE diskmap AS
SELECT
    i.start AS PhysicalStart,
    i.end AS PhysicalEnd,
    LIST({
        inode: e.Inode,
        path: e.Path,
        logical_start: e.LogicalStart + (i.start - e.PhysicalStart)
    }) AS Claimants
FROM intervals i
LEFT JOIN extents e
    ON e.PhysicalStart <= i.start
    AND e.PhysicalEnd >= i.end
GROUP BY i.start, i.end
ORDER BY i.start;
```

---

## Component 5: HTML Visualization

### Output Structure

```
diskmap/
├── index.html          # Viewer with all block elements
├── chunk-0.json        # Blocks 0 - 999999
├── chunk-1.json        # Blocks 1000000 - 1999999
└── ...
```

### Generator Class

```cpp
// include/diskmaphtml.h
#pragma once

#include <string>
#include <cstdint>

class LlamaDB;

class DiskMapHtmlGenerator {
public:
    DiskMapHtmlGenerator(LlamaDB& db, uint64_t blockSize = 4096);

    // Generate visualization to output directory
    bool generate(const std::string& outputDir, uint64_t chunksizeBlocks = 1000000);

private:
    void writeIndexHtml(const std::string& path, uint64_t totalBlocks, uint64_t numChunks);
    void writeChunk(const std::string& path, uint64_t chunkIndex, uint64_t startBlock, uint64_t endBlock);

    LlamaDB& Db;
    uint64_t BlockSize;
};
```

### HTML Template

```html
<!DOCTYPE html>
<html>
<head>
    <title>Disk Map Visualization</title>
    <style>
        body { background: #1a1a1a; color: #fff; font-family: monospace; margin: 0; }
        #diskmap { line-height: 0; }
        .block { width: 2px; height: 2px; display: inline-block; }
        .allocated { background: #4a9eff; }
        .overlap { background: #ff4a4a; }
        .unallocated { background: #444; }
        #info {
            position: fixed; bottom: 0; left: 0; right: 0;
            padding: 10px; background: #222; border-top: 1px solid #444;
        }
    </style>
</head>
<body>
    <div id="diskmap"></div>
    <div id="info">Hover over a block to see details</div>
    <script>
        const TOTAL_BLOCKS = {{TOTAL_BLOCKS}};
        const CHUNK_SIZE = {{CHUNK_SIZE}};
        const chunks = {};

        // Create all block elements
        const container = document.getElementById('diskmap');
        for (let i = 0; i < TOTAL_BLOCKS; i++) {
            const block = document.createElement('span');
            block.className = 'block unallocated';
            block.dataset.idx = i;
            container.appendChild(block);
        }

        // Lazy-load chunks on scroll
        const observer = new IntersectionObserver(entries => {
            entries.forEach(entry => {
                if (entry.isIntersecting) {
                    const idx = parseInt(entry.target.dataset.idx);
                    const chunkIdx = Math.floor(idx / CHUNK_SIZE);
                    loadChunk(chunkIdx);
                }
            });
        });

        document.querySelectorAll('.block').forEach(b => observer.observe(b));

        async function loadChunk(chunkIdx) {
            if (chunks[chunkIdx]) return;
            chunks[chunkIdx] = 'loading';

            const resp = await fetch(`chunk-${chunkIdx}.json`);
            const data = await resp.json();
            chunks[chunkIdx] = data;

            // Update block colors
            data.forEach(b => {
                const el = document.querySelector(`[data-idx="${b.idx}"]`);
                if (el) {
                    el.className = 'block ' + (b.claimants > 1 ? 'overlap' : 'allocated');
                    el.dataset.files = JSON.stringify(b.files);
                }
            });
        }

        // Hover info
        container.addEventListener('mouseover', e => {
            if (e.target.classList.contains('block')) {
                const files = JSON.parse(e.target.dataset.files || '[]');
                const idx = e.target.dataset.idx;
                document.getElementById('info').textContent =
                    `Block ${idx}: ` + (files.length ? files.join(', ') : 'Unallocated');
            }
        });
    </script>
</body>
</html>
```

### Usage

```bash
# After llama generates diskmap/
cd diskmap && python3 -m http.server 8000
# Open http://localhost:8000 in browser
```

---

## File Changes Summary

| File | Action | Description |
|------|--------|-------------|
| `Dockerfile` | Modify | Add FUSE, filesystem tools, e01 build |
| `docker-build-deps.sh` | Modify | Add e01/e01mount build steps |
| `include/extent.h` | Create | Extent struct with ColNames |
| `include/duckextent.h` | Create | ExtentBatch type alias |
| `include/posixreader.h` | Create | Linux-only PosixReader class |
| `src/posixreader.cpp` | Create | FIEMAP implementation |
| `include/diskmaphtml.h` | Create | HTML generator class |
| `src/diskmaphtml.cpp` | Create | HTML generator implementation |
| `meson.build` | Modify | Add new sources (Linux-conditional) |

---

## Testing Strategy

### Unit Tests
- `flagsToString()` - Known bitmask → expected string
- `statToInode()` - Constructed stat structs → Inode validation
- `parseExtents()` - Captured/synthetic FIEMAP buffers → Extent validation

### Integration Tests
- Full walk on small test filesystem in Docker
- Verify extent count matches expected
- Verify DuckDB table populated correctly

### Demo Test
- E01 → e01mount → loop mount → PosixReader → visualization
- Run through full workflow before talk

---

## Out of Scope (Future Work)

- FIBMAP fallback (not needed for target filesystems)
- ZFS support (kernel module complexity)
- Journal parsing (deferred, shown as extension point)
- Windows/macOS POSIX variants

---

**End of Design**
