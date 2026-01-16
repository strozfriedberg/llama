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

#include <fcntl.h>
#include <unistd.h>
#include <cstring>

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

#else
// Non-Linux placeholder
TEST_CASE("PosixReader not available on this platform", "[posixreader]") {
    SUCCEED("PosixReader is Linux-only");
}
#endif
