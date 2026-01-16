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
