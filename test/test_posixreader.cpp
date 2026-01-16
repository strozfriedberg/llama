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
