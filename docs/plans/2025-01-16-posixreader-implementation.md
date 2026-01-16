# PosixReader and Disk Map Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a Linux FIEMAP-based filesystem walker that extracts extent maps to DuckDB, with Norton Disk Doctor-style HTML visualization for DFIR Bayou demo.

**Architecture:** PosixReader walks a mounted filesystem using POSIX APIs and FIEMAP ioctl to extract physical extent information. Extents are batched to DuckDB using the existing boost::pfr reflection pattern. Sweep-line SQL queries construct the disk map. A separate HTML generator creates a chunked visualization.

**Tech Stack:** C++17, Linux FIEMAP ioctl, DuckDB, boost::pfr, Docker with FUSE

---

## Track A: Docker and Build Infrastructure

### Task A1: Update Dockerfile with FUSE and Filesystem Tools

**Files:**
- Modify: `Dockerfile`

**Step 1: Read current Dockerfile**

Review current state to understand where to add new packages.

**Step 2: Add FUSE and filesystem packages**

Add after the existing apt-get install block (around line 10-31):

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

**Step 3: Commit**

```bash
git add Dockerfile
git commit -m "Add FUSE and filesystem tools to Dockerfile"
```

---

### Task A2: Add e01 Build to docker-build-deps.sh

**Files:**
- Modify: `docker-build-deps.sh`

**Step 1: Read current script**

Review to understand the build pattern and where to add e01.

**Step 2: Add e01 build section**

Add before the llama build section (before line 103):

```bash
# Build e01 library and e01mount FUSE driver
if [ -d /code/e01 ]; then
    echo "Building e01..."
    cd /code/e01
    cargo build --release
    echo "Building e01mount..."
    cargo build --package e01mount --release
    echo "e01mount built at: /code/e01/target/release/e01mount"
else
    echo "WARNING: /code/e01 not found, skipping e01 build"
fi
```

**Step 3: Commit**

```bash
git add docker-build-deps.sh
git commit -m "Add e01 and e01mount build to Docker build script"
```

---

### Task A3: Build and Test Docker Image

**Step 1: Build the Docker image**

```bash
cd /Users/jonstewart/code/llama
docker build --platform linux/arm64 -t llama:latest .
```

Expected: Build completes successfully

**Step 2: Run container and test builds**

```bash
docker run --platform linux/arm64 -it --rm \
    --privileged \
    --device /dev/fuse \
    -v ~/code:/code \
    -v ~/ev:/ev:ro \
    llama:latest \
    /build/build-all.sh
```

Expected: All builds complete (sleuthkit, lightgrep, hasher, pdf_extractor, e01, llama)

**Step 3: Verify e01mount exists**

```bash
docker run --platform linux/arm64 -it --rm \
    -v ~/code:/code \
    llama:latest \
    ls -la /code/e01/target/release/e01mount
```

Expected: e01mount binary exists

**Step 4: Commit any fixes needed**

If build issues discovered, fix and commit.

---

### Task A4: Test FUSE Mounting Inside Container

**Step 1: Start interactive container**

```bash
docker run --platform linux/arm64 -it --rm \
    --privileged \
    --device /dev/fuse \
    -v ~/code:/code \
    -v ~/ev:/ev:ro \
    llama:latest \
    bash
```

**Step 2: Create mount point and test e01mount**

Inside container:
```bash
mkdir -p /tmp/e01mnt
# Assuming a test E01 exists in /ev
/code/e01/target/release/e01mount /ev/test.E01 /tmp/e01mnt -f &
sleep 2
ls -la /tmp/e01mnt
```

Expected: See `disk`, `disk-part1`, etc. files

**Step 3: Test loop mounting a partition**

```bash
mkdir -p /tmp/fs
mount -o ro,loop /tmp/e01mnt/disk-part1 /tmp/fs
ls /tmp/fs
```

Expected: Filesystem contents visible

**Step 4: Document any issues**

If issues found, document and fix.

---

## Track B: PosixReader Implementation

### Task B1: Create Extent Struct

**Files:**
- Create: `include/extent.h`
- Test: `test/test_extent.cpp`

**Step 1: Write the test**

Create `test/test_extent.cpp`:

```cpp
// ABOUTME: Unit tests for Extent struct
// ABOUTME: Verifies struct layout matches DuckDB expectations

#include <catch2/catch_test_macros.hpp>
#include "extent.h"
#include <boost/pfr.hpp>

TEST_CASE("Extent struct has correct number of fields", "[extent]") {
    Extent e{};
    constexpr auto numFields = boost::pfr::tuple_size_v<Extent>;
    REQUIRE(numFields == 9);
    REQUIRE(Extent::ColNames.size() == 9);
}

TEST_CASE("Extent struct can be constructed", "[extent]") {
    Extent e{
        .PhysicalStart = 0,
        .PhysicalEnd = 4096,
        .LogicalStart = 0,
        .LogicalEnd = 4096,
        .Inode = 12345,
        .FilesystemOffset = 0,
        .Path = "/test/file.txt",
        .Flags = "SHARED,ENCODED",
        .Source = "filesystem"
    };

    REQUIRE(e.PhysicalStart == 0);
    REQUIRE(e.PhysicalEnd == 4096);
    REQUIRE(e.Path == "/test/file.txt");
    REQUIRE(e.Flags == "SHARED,ENCODED");
}
```

**Step 2: Run test to verify it fails**

```bash
cd /code/llama && meson compile -C builddir && meson test -C builddir test_extent
```

Expected: FAIL - extent.h not found

**Step 3: Create extent.h**

Create `include/extent.h`:

```cpp
// ABOUTME: Extent struct representing a physical disk extent
// ABOUTME: Maps logical file offsets to physical disk locations via FIEMAP

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

**Step 4: Add to meson.build**

Add test to `test/meson.build`:

```meson
test_extent = executable(
    'test_extent',
    'test_extent.cpp',
    include_directories: incdir,
    dependencies: [catch2_dep, boost_dep],
)
test('test_extent', test_extent)
```

**Step 5: Run test to verify it passes**

```bash
meson compile -C builddir && meson test -C builddir test_extent
```

Expected: PASS

**Step 6: Commit**

```bash
git add include/extent.h test/test_extent.cpp test/meson.build
git commit -m "Add Extent struct for FIEMAP disk mapping"
```

---

### Task B2: Create DuckDB ExtentBatch

**Files:**
- Create: `include/duckextent.h`
- Test: `test/test_duckdb.cpp` (modify existing)

**Step 1: Read existing duckinode.h and test_duckdb.cpp**

Understand the pattern.

**Step 2: Write a test for ExtentBatch**

Add to `test/test_duckdb.cpp`:

```cpp
#include "duckextent.h"

TEST_CASE("ExtentBatch can add and retrieve extents", "[duckdb]") {
    ExtentBatch batch;

    Extent e{
        .PhysicalStart = 1000,
        .PhysicalEnd = 2000,
        .LogicalStart = 0,
        .LogicalEnd = 1000,
        .Inode = 42,
        .FilesystemOffset = 0,
        .Path = "/test.txt",
        .Flags = "SHARED",
        .Source = "filesystem"
    };

    batch.add(e);
    REQUIRE(batch.size() == 1);

    batch.add(e);
    REQUIRE(batch.size() == 2);

    batch.clear();
    REQUIRE(batch.size() == 0);
}
```

**Step 3: Run test to verify it fails**

```bash
meson compile -C builddir && meson test -C builddir test_duckdb
```

Expected: FAIL - duckextent.h not found

**Step 4: Create duckextent.h**

Create `include/duckextent.h`:

```cpp
// ABOUTME: DuckDB batch type for Extent records
// ABOUTME: Uses boost::pfr reflection for serialization

#pragma once

#include "extent.h"
#include "llamaduck.h"

using ExtentBatch = DBBatch<Extent>;
```

**Step 5: Run test to verify it passes**

```bash
meson compile -C builddir && meson test -C builddir test_duckdb
```

Expected: PASS

**Step 6: Commit**

```bash
git add include/duckextent.h test/test_duckdb.cpp
git commit -m "Add ExtentBatch for DuckDB extent serialization"
```

---

### Task B3: Implement flagsToString

**Files:**
- Create: `include/posixreader.h` (partial)
- Create: `src/posixreader.cpp` (partial)
- Test: `test/test_posixreader.cpp`

**Step 1: Write the test**

Create `test/test_posixreader.cpp`:

```cpp
// ABOUTME: Unit tests for PosixReader
// ABOUTME: Tests FIEMAP flag conversion and extent parsing

#include <catch2/catch_test_macros.hpp>

#ifdef __linux__
#include "posixreader.h"
#include <linux/fiemap.h>

TEST_CASE("flagsToString converts empty flags", "[posixreader]") {
    REQUIRE(PosixReader::flagsToString(0) == "");
}

TEST_CASE("flagsToString converts single flag", "[posixreader]") {
    REQUIRE(PosixReader::flagsToString(FIEMAP_EXTENT_LAST) == "LAST");
    REQUIRE(PosixReader::flagsToString(FIEMAP_EXTENT_SHARED) == "SHARED");
    REQUIRE(PosixReader::flagsToString(FIEMAP_EXTENT_ENCODED) == "ENCODED");
}

TEST_CASE("flagsToString converts multiple flags", "[posixreader]") {
    uint32_t flags = FIEMAP_EXTENT_SHARED | FIEMAP_EXTENT_ENCODED;
    std::string result = PosixReader::flagsToString(flags);
    // Order depends on implementation, check both are present
    REQUIRE(result.find("SHARED") != std::string::npos);
    REQUIRE(result.find("ENCODED") != std::string::npos);
    REQUIRE(result.find(",") != std::string::npos);
}

TEST_CASE("flagsToString handles UNWRITTEN flag", "[posixreader]") {
    REQUIRE(PosixReader::flagsToString(FIEMAP_EXTENT_UNWRITTEN) == "UNWRITTEN");
}

#else
// Non-Linux placeholder
TEST_CASE("PosixReader not available on this platform", "[posixreader]") {
    SUCCEED("PosixReader is Linux-only");
}
#endif
```

**Step 2: Run test to verify it fails**

```bash
meson compile -C builddir && meson test -C builddir test_posixreader
```

Expected: FAIL - posixreader.h not found (or passes with placeholder on macOS)

**Step 3: Create posixreader.h with flagsToString declaration**

Create `include/posixreader.h`:

```cpp
// ABOUTME: Linux-only InputReader using POSIX APIs and FIEMAP
// ABOUTME: Extracts physical extent information for disk mapping

#pragma once

#ifdef __linux__

#include "inputreader.h"
#include "direntstack.h"
#include "extent.h"
#include "inode.h"
#include "recordhasher.h"

#include <sys/stat.h>
#include <filesystem>
#include <vector>

class InputHandler;
class OutputHandler;

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

**Step 4: Create posixreader.cpp with flagsToString implementation**

Create `src/posixreader.cpp`:

```cpp
// ABOUTME: Linux-only InputReader using POSIX APIs and FIEMAP
// ABOUTME: Walks mounted filesystems extracting extent information

#ifdef __linux__

#include "posixreader.h"

#include <linux/fiemap.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

PosixReader::PosixReader(const std::string& mountpoint)
    : Mountpoint(mountpoint)
    , FsOffset(0)
    , Input()
    , Output()
    , RecHasher()
    , Dirents(RecHasher)
{
}

PosixReader::~PosixReader() = default;

void PosixReader::setInputHandler(const std::shared_ptr<InputHandler>& in) {
    Input = in;
}

void PosixReader::setOutputHandler(const std::shared_ptr<OutputHandler>& out) {
    Output = out;
}

bool PosixReader::startReading() {
    // TODO: Implement in later task
    return false;
}

std::string PosixReader::flagsToString(uint32_t flags) {
    std::string result;

    if (flags & FIEMAP_EXTENT_LAST)           { result += "LAST,"; }
    if (flags & FIEMAP_EXTENT_UNKNOWN)        { result += "UNKNOWN,"; }
    if (flags & FIEMAP_EXTENT_DELALLOC)       { result += "DELALLOC,"; }
    if (flags & FIEMAP_EXTENT_ENCODED)        { result += "ENCODED,"; }
    if (flags & FIEMAP_EXTENT_DATA_ENCRYPTED) { result += "ENCRYPTED,"; }
    if (flags & FIEMAP_EXTENT_NOT_ALIGNED)    { result += "NOT_ALIGNED,"; }
    if (flags & FIEMAP_EXTENT_DATA_INLINE)    { result += "INLINE,"; }
    if (flags & FIEMAP_EXTENT_DATA_TAIL)      { result += "TAIL,"; }
    if (flags & FIEMAP_EXTENT_UNWRITTEN)      { result += "UNWRITTEN,"; }
    if (flags & FIEMAP_EXTENT_MERGED)         { result += "MERGED,"; }
    if (flags & FIEMAP_EXTENT_SHARED)         { result += "SHARED,"; }

    // Remove trailing comma
    if (!result.empty() && result.back() == ',') {
        result.pop_back();
    }

    return result;
}

// Placeholder implementations - filled in later tasks
Inode PosixReader::statToInode(const struct stat& st, const std::string& path) {
    Inode inode;
    // TODO: Implement in later task
    return inode;
}

void PosixReader::parseExtents(
    const uint8_t* fiemapBuf,
    size_t bufLen,
    uint64_t inode,
    const std::string& path,
    uint64_t fsOffset,
    std::vector<Extent>& extentsOut)
{
    // TODO: Implement in later task
    extentsOut.clear();
}

bool PosixReader::callFiemap(int fd, std::vector<uint8_t>& bufOut) {
    // TODO: Implement in later task
    bufOut.clear();
    return false;
}

void PosixReader::walkFilesystem() {
    // TODO: Implement in later task
}

void PosixReader::handleEntry(const std::filesystem::directory_entry& entry) {
    // TODO: Implement in later task
}

#endif // __linux__
```

**Step 5: Add to meson.build**

Add to `src/meson.build` (Linux-conditional):

```meson
if host_machine.system() == 'linux'
    llama_sources += files('posixreader.cpp')
endif
```

Add test to `test/meson.build`:

```meson
test_posixreader = executable(
    'test_posixreader',
    'test_posixreader.cpp',
    include_directories: incdir,
    dependencies: [catch2_dep],
)
test('test_posixreader', test_posixreader)
```

**Step 6: Run test in Docker (Linux)**

```bash
docker run --platform linux/arm64 -it --rm \
    -v ~/code:/code \
    llama:latest \
    bash -c "cd /code/llama && meson compile -C builddir && meson test -C builddir test_posixreader -v"
```

Expected: PASS

**Step 7: Commit**

```bash
git add include/posixreader.h src/posixreader.cpp test/test_posixreader.cpp src/meson.build test/meson.build
git commit -m "Add PosixReader with flagsToString implementation"
```

---

### Task B4: Implement callFiemap

**Files:**
- Modify: `src/posixreader.cpp`
- Modify: `test/test_posixreader.cpp`

**Step 1: Write integration test**

Add to `test/test_posixreader.cpp`:

```cpp
#ifdef __linux__
#include <fcntl.h>
#include <unistd.h>

TEST_CASE("callFiemap returns data for regular file", "[posixreader][integration]") {
    // Create a temp file with some content
    char tmpfile[] = "/tmp/fiemap_test_XXXXXX";
    int fd = mkstemp(tmpfile);
    REQUIRE(fd >= 0);

    // Write some data
    const char* data = "Hello FIEMAP test data";
    write(fd, data, strlen(data));
    fsync(fd);

    // Call FIEMAP
    std::vector<uint8_t> buf;
    bool result = PosixReader::callFiemap(fd, buf);

    close(fd);
    unlink(tmpfile);

    // FIEMAP may or may not work on tmpfs, but shouldn't crash
    if (result) {
        REQUIRE(buf.size() >= sizeof(fiemap));
        fiemap* fm = reinterpret_cast<fiemap*>(buf.data());
        // May have 0 extents on tmpfs (inline data), that's ok
        REQUIRE(fm->fm_mapped_extents >= 0);
    }
}
#endif
```

**Step 2: Run test to see current behavior**

```bash
docker run ... meson test -C builddir test_posixreader -v
```

Expected: Test may fail or pass with empty buffer

**Step 3: Implement callFiemap**

Replace the placeholder in `src/posixreader.cpp`:

```cpp
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

**Step 4: Run test to verify**

```bash
docker run ... meson test -C builddir test_posixreader -v
```

Expected: PASS

**Step 5: Commit**

```bash
git add src/posixreader.cpp test/test_posixreader.cpp
git commit -m "Implement callFiemap with iteration for large files"
```

---

### Task B5: Implement parseExtents

**Files:**
- Modify: `src/posixreader.cpp`
- Modify: `test/test_posixreader.cpp`

**Step 1: Write the test**

Add to `test/test_posixreader.cpp`:

```cpp
#ifdef __linux__
TEST_CASE("parseExtents converts FIEMAP buffer to Extent records", "[posixreader]") {
    // Create a synthetic FIEMAP buffer
    std::vector<uint8_t> buf(sizeof(fiemap) + 2 * sizeof(fiemap_extent));

    fiemap* fm = reinterpret_cast<fiemap*>(buf.data());
    fm->fm_mapped_extents = 2;

    fiemap_extent* ext0 = &fm->fm_extents[0];
    ext0->fe_logical = 0;
    ext0->fe_physical = 1000;
    ext0->fe_length = 4096;
    ext0->fe_flags = 0;

    fiemap_extent* ext1 = &fm->fm_extents[1];
    ext1->fe_logical = 4096;
    ext1->fe_physical = 8000;
    ext1->fe_length = 4096;
    ext1->fe_flags = FIEMAP_EXTENT_LAST | FIEMAP_EXTENT_SHARED;

    std::vector<Extent> extents;
    PosixReader::parseExtents(buf.data(), buf.size(), 42, "/test.txt", 0, extents);

    REQUIRE(extents.size() == 2);

    REQUIRE(extents[0].LogicalStart == 0);
    REQUIRE(extents[0].LogicalEnd == 4096);
    REQUIRE(extents[0].PhysicalStart == 1000);
    REQUIRE(extents[0].PhysicalEnd == 1000 + 4096);
    REQUIRE(extents[0].Inode == 42);
    REQUIRE(extents[0].Path == "/test.txt");
    REQUIRE(extents[0].Flags == "");

    REQUIRE(extents[1].LogicalStart == 4096);
    REQUIRE(extents[1].PhysicalStart == 8000);
    REQUIRE(extents[1].Flags.find("LAST") != std::string::npos);
    REQUIRE(extents[1].Flags.find("SHARED") != std::string::npos);
}
#endif
```

**Step 2: Run test to verify it fails**

Expected: FAIL - parseExtents returns empty vector

**Step 3: Implement parseExtents**

Replace placeholder in `src/posixreader.cpp`:

```cpp
void PosixReader::parseExtents(
    const uint8_t* fiemapBuf,
    size_t bufLen,
    uint64_t inode,
    const std::string& path,
    uint64_t fsOffset,
    std::vector<Extent>& extentsOut)
{
    extentsOut.clear();

    if (bufLen < sizeof(fiemap)) {
        return;
    }

    const fiemap* fm = reinterpret_cast<const fiemap*>(fiemapBuf);
    const size_t expectedSize = sizeof(fiemap) + fm->fm_mapped_extents * sizeof(fiemap_extent);

    if (bufLen < expectedSize) {
        return;
    }

    extentsOut.reserve(fm->fm_mapped_extents);

    for (uint32_t i = 0; i < fm->fm_mapped_extents; ++i) {
        const fiemap_extent& fe = fm->fm_extents[i];

        Extent ext;
        ext.LogicalStart = fe.fe_logical;
        ext.LogicalEnd = fe.fe_logical + fe.fe_length;
        ext.PhysicalStart = fe.fe_physical;
        ext.PhysicalEnd = fe.fe_physical + fe.fe_length;
        ext.Inode = inode;
        ext.FilesystemOffset = fsOffset;
        ext.Path = path;
        ext.Flags = flagsToString(fe.fe_flags);
        ext.Source = "filesystem";

        extentsOut.push_back(std::move(ext));
    }
}
```

**Step 4: Run test to verify it passes**

Expected: PASS

**Step 5: Commit**

```bash
git add src/posixreader.cpp test/test_posixreader.cpp
git commit -m "Implement parseExtents for FIEMAP buffer conversion"
```

---

### Task B6: Implement statToInode

**Files:**
- Modify: `src/posixreader.cpp`
- Modify: `test/test_posixreader.cpp`

**Step 1: Write the test**

Add to `test/test_posixreader.cpp`:

```cpp
#ifdef __linux__
TEST_CASE("statToInode converts stat struct to Inode", "[posixreader]") {
    struct stat st = {};
    st.st_ino = 12345;
    st.st_mode = S_IFREG | 0644;
    st.st_size = 1024;
    st.st_uid = 1000;
    st.st_gid = 1000;
    st.st_nlink = 1;
    st.st_mtim.tv_sec = 1705400000;
    st.st_atim.tv_sec = 1705400001;
    st.st_ctim.tv_sec = 1705400002;

    Inode inode = PosixReader::statToInode(st, "/test/file.txt");

    REQUIRE(inode.Addr == 12345);
    REQUIRE(inode.Filesize == 1024);
    REQUIRE(inode.Uid == 1000);
    REQUIRE(inode.Gid == 1000);
    REQUIRE(inode.NumLinks == 1);
    REQUIRE(inode.Type == "file");
}

TEST_CASE("statToInode identifies directories", "[posixreader]") {
    struct stat st = {};
    st.st_mode = S_IFDIR | 0755;

    Inode inode = PosixReader::statToInode(st, "/test/dir");

    REQUIRE(inode.Type == "directory");
}

TEST_CASE("statToInode identifies symlinks", "[posixreader]") {
    struct stat st = {};
    st.st_mode = S_IFLNK | 0777;

    Inode inode = PosixReader::statToInode(st, "/test/link");

    REQUIRE(inode.Type == "symlink");
}
#endif
```

**Step 2: Run test to verify it fails**

Expected: FAIL - returns empty Inode

**Step 3: Implement statToInode**

Replace placeholder in `src/posixreader.cpp`:

```cpp
Inode PosixReader::statToInode(const struct stat& st, const std::string& path) {
    Inode inode;

    inode.Addr = st.st_ino;
    inode.FsOffset = 0;  // Set by caller if needed
    inode.Filesize = st.st_size;
    inode.Uid = st.st_uid;
    inode.Gid = st.st_gid;
    inode.NumLinks = st.st_nlink;
    inode.SeqNum = 0;

    // Determine type
    if (S_ISREG(st.st_mode)) {
        inode.Type = "file";
    } else if (S_ISDIR(st.st_mode)) {
        inode.Type = "directory";
    } else if (S_ISLNK(st.st_mode)) {
        inode.Type = "symlink";
    } else if (S_ISBLK(st.st_mode)) {
        inode.Type = "block";
    } else if (S_ISCHR(st.st_mode)) {
        inode.Type = "char";
    } else if (S_ISFIFO(st.st_mode)) {
        inode.Type = "fifo";
    } else if (S_ISSOCK(st.st_mode)) {
        inode.Type = "socket";
    } else {
        inode.Type = "unknown";
    }

    // Flags from mode
    inode.Flags = "";

    // Timestamps - convert to ISO8601 strings
    auto formatTime = [](time_t t) -> std::string {
        if (t == 0) return "";
        char buf[32];
        struct tm tm;
        gmtime_r(&t, &tm);
        strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
        return buf;
    };

    inode.Modified = formatTime(st.st_mtim.tv_sec);
    inode.Accessed = formatTime(st.st_atim.tv_sec);
    inode.Metadata = formatTime(st.st_ctim.tv_sec);
    inode.Created = "";  // POSIX doesn't have birth time

    inode.LinkTarget = "";  // Caller can fill if symlink
    inode.Id = "";  // Caller can generate hash

    return inode;
}
```

**Step 4: Run test to verify it passes**

Expected: PASS

**Step 5: Commit**

```bash
git add src/posixreader.cpp test/test_posixreader.cpp
git commit -m "Implement statToInode for POSIX stat conversion"
```

---

### Task B7: Implement startReading (Filesystem Walk)

**Files:**
- Modify: `src/posixreader.cpp`
- Integration test in Docker

**Step 1: Implement walkFilesystem and handleEntry**

Update `src/posixreader.cpp`:

```cpp
#include "inputhandler.h"
#include "duckextent.h"

bool PosixReader::startReading() {
    walkFilesystem();

    // Flush remaining dirents
    while (!Dirents.empty()) {
        Input->push(Dirents.pop());
    }

    Input->flush();
    return true;
}

void PosixReader::walkFilesystem() {
    namespace fs = std::filesystem;

    std::vector<uint8_t> fiemapBuf;
    std::vector<Extent> extents;

    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(Mountpoint, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator();
         ++it)
    {
        if (ec) {
            ec.clear();
            continue;
        }

        handleEntry(*it);

        // For regular files, get FIEMAP data
        if (it->is_regular_file(ec) && !ec) {
            const std::string pathStr = it->path().string();
            int fd = open(pathStr.c_str(), O_RDONLY);
            if (fd >= 0) {
                struct stat st;
                if (fstat(fd, &st) == 0) {
                    if (callFiemap(fd, fiemapBuf)) {
                        parseExtents(fiemapBuf.data(), fiemapBuf.size(),
                                     st.st_ino, pathStr, FsOffset, extents);
                        // TODO: Add extents to batch and flush to DuckDB
                        for (const auto& ext : extents) {
                            // For now, just process - DuckDB integration in next task
                        }
                    }
                }
                close(fd);
            }
        }
    }
}

void PosixReader::handleEntry(const std::filesystem::directory_entry& entry) {
    std::error_code ec;
    struct stat st;

    if (lstat(entry.path().c_str(), &st) != 0) {
        return;
    }

    Inode inode = statToInode(st, entry.path().string());
    Input->push(inode);

    // TODO: Handle dirents similar to TskReader/DirReader
}
```

**Step 2: Test in Docker on a real filesystem**

```bash
docker run --platform linux/arm64 -it --rm \
    --privileged \
    -v ~/code:/code \
    llama:latest \
    bash -c "cd /code/llama && meson compile -C builddir"
```

**Step 3: Commit**

```bash
git add src/posixreader.cpp
git commit -m "Implement startReading filesystem walk with FIEMAP"
```

---

### Task B8: Integrate DuckDB Extent Table

**Files:**
- Modify: `src/posixreader.cpp`
- Modify: `include/posixreader.h`

**Step 1: Add ExtentBatch to PosixReader**

Update `include/posixreader.h` private section:

```cpp
private:
    // ... existing members ...
    ExtentBatch ExtentsBatch;
    std::shared_ptr<LlamaDBAppender> ExtentAppender;
```

**Step 2: Update startReading to flush extents**

Update the walk loop in `src/posixreader.cpp`:

```cpp
static constexpr size_t EXTENT_BATCH_FLUSH_SIZE = 10000;

// In walkFilesystem(), after parseExtents:
for (const auto& ext : extents) {
    ExtentsBatch.add(ext);
    if (ExtentsBatch.size() >= EXTENT_BATCH_FLUSH_SIZE) {
        ExtentsBatch.copyToDB(ExtentAppender->get());
        ExtentsBatch.clear();
    }
}

// At end of walkFilesystem:
if (ExtentsBatch.size() > 0) {
    ExtentsBatch.copyToDB(ExtentAppender->get());
    ExtentsBatch.clear();
}
```

**Step 3: Test integration**

Run full walk on test filesystem in Docker.

**Step 4: Commit**

```bash
git add include/posixreader.h src/posixreader.cpp
git commit -m "Integrate DuckDB ExtentBatch for extent storage"
```

---

## Track C: Visualization

### Task C1: Create DiskMapHtmlGenerator

**Files:**
- Create: `include/diskmaphtml.h`
- Create: `src/diskmaphtml.cpp`

**Step 1: Create header**

Create `include/diskmaphtml.h`:

```cpp
// ABOUTME: Generates Norton Disk Doctor-style HTML visualization
// ABOUTME: Creates chunked JSON files for lazy-loading large disk maps

#pragma once

#include <string>
#include <cstdint>

class LlamaDB;
class LlamaDBConnection;

class DiskMapHtmlGenerator {
public:
    DiskMapHtmlGenerator(LlamaDBConnection& conn, uint64_t blockSize = 4096);

    // Generate visualization to output directory
    // Returns true on success
    bool generate(const std::string& outputDir, uint64_t chunkSizeBlocks = 1000000);

private:
    bool writeIndexHtml(const std::string& path, uint64_t totalBlocks, uint64_t numChunks);
    bool writeChunk(const std::string& dir, uint64_t chunkIndex,
                    uint64_t startBlock, uint64_t endBlock);

    LlamaDBConnection& Conn;
    uint64_t BlockSize;
};
```

**Step 2: Implement**

Create `src/diskmaphtml.cpp` with full implementation (HTML template + JSON generation from diskmap table).

**Step 3: Add to meson.build**

**Step 4: Test on sample data**

**Step 5: Commit**

```bash
git add include/diskmaphtml.h src/diskmaphtml.cpp src/meson.build
git commit -m "Add DiskMapHtmlGenerator for visualization"
```

---

### Task C2: Add SQL Queries for Disk Map Construction

**Files:**
- Create: `sql/diskmap_queries.sql` (reference)
- Integrate queries into C++ or llama CLI

**Step 1: Document the queries**

```sql
-- Create boundaries table
CREATE TEMP TABLE boundaries AS
SELECT DISTINCT PhysicalStart AS pos FROM extents
UNION
SELECT DISTINCT PhysicalEnd AS pos FROM extents
ORDER BY pos;

-- Create intervals table
CREATE TEMP TABLE intervals AS
SELECT
    pos AS start,
    LEAD(pos) OVER (ORDER BY pos) AS end
FROM boundaries
WHERE LEAD(pos) OVER (ORDER BY pos) IS NOT NULL;

-- Create diskmap with claimants
CREATE TABLE diskmap AS
SELECT
    i.start AS PhysicalStart,
    i.end AS PhysicalEnd,
    LIST({inode: e.Inode, path: e.Path}) AS Claimants
FROM intervals i
LEFT JOIN extents e
    ON e.PhysicalStart <= i.start
    AND e.PhysicalEnd >= i.end
GROUP BY i.start, i.end
ORDER BY i.start;
```

**Step 2: Add to llama CLI or create separate tool**

**Step 3: Test on sample data**

**Step 4: Commit**

---

## Verification Checklist

Before demo:

- [ ] Docker image builds successfully
- [ ] e01mount works inside container
- [ ] Loop mounting partitions works
- [ ] PosixReader walks filesystem without errors
- [ ] FIEMAP returns extent data on test filesystem
- [ ] Extents written to DuckDB
- [ ] Sweep-line SQL produces diskmap table
- [ ] HTML visualization generates and displays
- [ ] Full E01 → visualization pipeline works end-to-end

---

**End of Implementation Plan**
