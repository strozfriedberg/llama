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
