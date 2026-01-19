// ABOUTME: Generates Norton Disk Doctor-style HTML visualization
// ABOUTME: Creates chunked JSON files for lazy-loading large disk maps

#include "diskmaphtml.h"
#include "jsoncons_wrapper.h"
#include "llamaduck.h"
#include "throw.h"

#include <duckdb.h>
#include <fstream>
#include <sstream>
#include <filesystem>

DiskMapHtmlGenerator::DiskMapHtmlGenerator(LlamaDBConnection& conn, uint64_t blockSize)
    : Conn(conn)
    , BlockSize(blockSize)
{
}

bool DiskMapHtmlGenerator::generate(const std::string& outputDir, uint64_t chunkSizeBlocks) {
    // Create output directory if it doesn't exist
    std::filesystem::create_directories(outputDir);

    // Query to get total disk size
    duckdb_result result;
    auto state = duckdb_query(Conn.get(),
        "SELECT MAX(PhysicalEnd) as MaxOffset FROM diskmap;",
        &result);

    if (state == DuckDBError) {
        return false;
    }

    uint64_t totalBytes = 0;
    if (duckdb_row_count(&result) > 0) {
        totalBytes = duckdb_value_uint64(&result, 0, 0);
    }
    duckdb_destroy_result(&result);

    uint64_t totalBlocks = (totalBytes + BlockSize - 1) / BlockSize;
    uint64_t numChunks = (totalBlocks + chunkSizeBlocks - 1) / chunkSizeBlocks;

    // Write index.html
    std::string indexPath = outputDir + "/index.html";
    if (!writeIndexHtml(indexPath, totalBlocks, numChunks)) {
        return false;
    }

    // Write chunk files
    for (uint64_t i = 0; i < numChunks; ++i) {
        uint64_t startBlock = i * chunkSizeBlocks;
        uint64_t endBlock = std::min(startBlock + chunkSizeBlocks, totalBlocks);
        if (!writeChunk(outputDir, i, startBlock, endBlock)) {
            return false;
        }
    }

    return true;
}

bool DiskMapHtmlGenerator::writeIndexHtml(const std::string& path, uint64_t totalBlocks, uint64_t numChunks) {
    std::ofstream out(path);
    if (!out) {
        return false;
    }

    out << R"HTML(<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Disk Map Visualization</title>
    <style>
        body {
            font-family: monospace;
            margin: 0;
            padding: 20px;
            background: #000;
            color: #0f0;
        }
        #diskmap {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(8px, 1fr));
            gap: 1px;
            width: 100%;
            max-width: 1200px;
        }
        .block {
            width: 8px;
            height: 8px;
            cursor: pointer;
        }
        .free { background: #003; }
        .allocated { background: #0f0; }
        .multi { background: #f00; }
        #info {
            position: fixed;
            top: 10px;
            right: 10px;
            background: rgba(0,0,0,0.8);
            padding: 10px;
            border: 1px solid #0f0;
            max-width: 400px;
            display: none;
        }
        #legend {
            margin-bottom: 20px;
        }
        .legend-item {
            display: inline-block;
            margin-right: 20px;
        }
        .legend-box {
            display: inline-block;
            width: 16px;
            height: 16px;
            vertical-align: middle;
            margin-right: 5px;
        }
    </style>
</head>
<body>
    <h1>Disk Map Visualization</h1>
    <div id="legend">
        <div class="legend-item"><span class="legend-box free"></span>Free Space</div>
        <div class="legend-item"><span class="legend-box allocated"></span>Allocated</div>
        <div class="legend-item"><span class="legend-box multi"></span>Multiple Claimants</div>
    </div>
    <div id="stats">
        Total Blocks: )HTML" << totalBlocks << R"HTML(<br>
        Total Chunks: )HTML" << numChunks << R"HTML(<br>
        Loading...
    </div>
    <div id="diskmap"></div>
    <div id="info"></div>
    <script>
        const totalBlocks = )HTML" << totalBlocks << R"HTML(;
        const numChunks = )HTML" << numChunks << R"HTML(;
        const diskmap = document.getElementById('diskmap');
        const info = document.getElementById('info');

        let chunks = [];

        async function loadChunks() {
            for (let i = 0; i < numChunks; i++) {
                const response = await fetch(`chunk_${i}.json`);
                const data = await response.json();
                chunks.push(data);
                renderChunk(data);
            }
            document.getElementById('stats').innerHTML =
                `Total Blocks: ${totalBlocks}<br>Loaded ${chunks.length} chunks`;
        }

        function renderChunk(chunk) {
            chunk.blocks.forEach(block => {
                const div = document.createElement('div');
                div.className = 'block';
                if (block.claimants.length === 0) {
                    div.classList.add('free');
                } else if (block.claimants.length === 1) {
                    div.classList.add('allocated');
                } else {
                    div.classList.add('multi');
                }
                div.dataset.start = block.start;
                div.dataset.end = block.end;
                div.dataset.claimants = JSON.stringify(block.claimants);
                div.addEventListener('click', showBlockInfo);
                diskmap.appendChild(div);
            });
        }

        function showBlockInfo(e) {
            const block = e.target;
            const claimants = JSON.parse(block.dataset.claimants);
            let html = `<strong>Block Range:</strong> ${block.dataset.start} - ${block.dataset.end}<br><br>`;

            if (claimants.length === 0) {
                html += '<strong>Status:</strong> Free';
            } else {
                html += `<strong>Claimants (${claimants.length}):</strong><br>`;
                claimants.forEach(c => {
                    html += `<div>Inode: ${c.inode}<br>Path: ${c.path}</div><br>`;
                });
            }

            info.innerHTML = html;
            info.style.display = 'block';
        }

        document.addEventListener('click', (e) => {
            if (!e.target.classList.contains('block')) {
                info.style.display = 'none';
            }
        });

        loadChunks();
    </script>
</body>
</html>
)HTML";

    return true;
}

bool DiskMapHtmlGenerator::writeChunk(const std::string& dir, uint64_t chunkIndex,
                                      uint64_t startBlock, uint64_t endBlock) {
    std::stringstream query;
    query << "SELECT PhysicalStart, PhysicalEnd, Claimants FROM diskmap "
          << "WHERE PhysicalStart >= " << (startBlock * BlockSize)
          << " AND PhysicalStart < " << (endBlock * BlockSize)
          << " ORDER BY PhysicalStart;";

    duckdb_result result;
    auto state = duckdb_query(Conn.get(), query.str().c_str(), &result);

    if (state == DuckDBError) {
        return false;
    }

    std::stringstream filename;
    filename << dir << "/chunk_" << chunkIndex << ".json";
    std::ofstream out(filename.str());
    if (!out) {
        duckdb_destroy_result(&result);
        return false;
    }

    // Build JSON structure using jsoncons
    jsoncons::json chunk = jsoncons::json::object();
    jsoncons::json blocks = jsoncons::json::array();

    uint64_t rowCount = duckdb_row_count(&result);
    for (uint64_t row = 0; row < rowCount; ++row) {
        uint64_t physStart = duckdb_value_uint64(&result, 0, row);
        uint64_t physEnd = duckdb_value_uint64(&result, 1, row);

        // Get claimants (comma-separated string from string_agg)
        auto claimantsStr = duckdb_value_varchar(&result, 2, row);

        jsoncons::json block = jsoncons::json::object();
        block["start"] = physStart;
        block["end"] = physEnd;

        jsoncons::json claimants = jsoncons::json::array();
        if (claimantsStr != nullptr && std::string(claimantsStr) != "NULL" && std::string(claimantsStr).length() > 0) {
            // Parse comma-separated paths
            std::string paths(claimantsStr);
            size_t start = 0;
            while (start < paths.length()) {
                size_t comma = paths.find(',', start);
                std::string path = (comma == std::string::npos)
                    ? paths.substr(start)
                    : paths.substr(start, comma - start);
                claimants.push_back(path);
                if (comma == std::string::npos) break;
                start = comma + 1;
            }
        }
        block["claimants"] = claimants;
        blocks.push_back(block);

        if (claimantsStr) {
            duckdb_free(claimantsStr);
        }
    }

    chunk["blocks"] = blocks;

    // Serialize to file
    out << chunk;

    duckdb_destroy_result(&result);
    return true;
}
