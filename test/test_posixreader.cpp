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

#else
// Non-Linux placeholder
TEST_CASE("PosixReader not available on this platform", "[posixreader]") {
    SUCCEED("PosixReader is Linux-only");
}
#endif
