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
