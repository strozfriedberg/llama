// ABOUTME: Simple test program to verify disk map generation and visualization
// ABOUTME: Creates synthetic extent data and tests the full pipeline

#include "diskmaphtml.h"
#include "duckextent.h"
#include "extent.h"
#include "llamaduck.h"

#include <iostream>
#include <filesystem>

int main() {
    std::cout << "=== Disk Map Visualization Test ===" << std::endl;

    // Create database
    LlamaDB db;
    LlamaDBConnection conn(db);

    // Create extents table
    if (!DBType<Extent>::createTable(conn.get(), "extents")) {
        std::cerr << "Failed to create extents table" << std::endl;
        return 1;
    }
    std::cout << "✓ Created extents table" << std::endl;

    // Create appender and add test extents
    LlamaDBAppender appender(conn.get(), "extents");
    ExtentBatch batch;

    // Add some synthetic extents simulating files on disk
    // File 1: 100KB at offset 0
    batch.add(Extent{
        .PhysicalStart = 0,
        .PhysicalEnd = 102400,
        .LogicalStart = 0,
        .LogicalEnd = 102400,
        .Inode = 1,
        .FilesystemOffset = 0,
        .Path = "/test/file1.txt",
        .Flags = "",
        .Source = "filesystem"
    });

    // File 2: 50KB at offset 102400
    batch.add(Extent{
        .PhysicalStart = 102400,
        .PhysicalEnd = 153600,
        .LogicalStart = 0,
        .LogicalEnd = 51200,
        .Inode = 2,
        .FilesystemOffset = 0,
        .Path = "/test/file2.txt",
        .Flags = "",
        .Source = "filesystem"
    });

    // File 3: Overlapping extent (simulating shared/deduped data)
    batch.add(Extent{
        .PhysicalStart = 102400,
        .PhysicalEnd = 122880,
        .LogicalStart = 0,
        .LogicalEnd = 20480,
        .Inode = 3,
        .FilesystemOffset = 0,
        .Path = "/test/file3.txt",
        .Flags = "SHARED",
        .Source = "filesystem"
    });

    // File 4: Large file with gap
    batch.add(Extent{
        .PhysicalStart = 204800,
        .PhysicalEnd = 409600,
        .LogicalStart = 0,
        .LogicalEnd = 204800,
        .Inode = 4,
        .FilesystemOffset = 0,
        .Path = "/test/largefile.dat",
        .Flags = "",
        .Source = "filesystem"
    });

    batch.copyToDB(appender.get());
    appender.flush();
    std::cout << "✓ Added " << batch.size() << " test extents" << std::endl;

    // Debug: Check extents
    duckdb_result result;
    auto state = duckdb_query(conn.get(), "SELECT PhysicalStart, PhysicalEnd, Path FROM extents;", &result);
    if (state != DuckDBError) {
        uint64_t rowCount = duckdb_row_count(&result);
        std::cout << "DEBUG: Extents table has " << rowCount << " rows:" << std::endl;
        for (uint64_t row = 0; row < rowCount; ++row) {
            uint64_t start = duckdb_value_uint64(&result, 0, row);
            uint64_t end = duckdb_value_uint64(&result, 1, row);
            auto path = duckdb_value_varchar(&result, 2, row);
            std::cout << "  " << start << " - " << end << " : " << (path ? path : "NULL") << std::endl;
            if (path) duckdb_free(path);
        }
        duckdb_destroy_result(&result);
    }

    // Create disk map using SQL queries

    // Step 1: Create boundaries
    state = duckdb_query(conn.get(),
        "CREATE TEMP TABLE boundaries AS "
        "SELECT DISTINCT PhysicalStart AS pos FROM extents "
        "UNION "
        "SELECT DISTINCT PhysicalEnd AS pos FROM extents "
        "ORDER BY pos;",
        &result);

    if (state == DuckDBError) {
        std::cerr << "Error creating boundaries table" << std::endl;
        return 1;
    }
    duckdb_destroy_result(&result);
    std::cout << "✓ Created boundaries table" << std::endl;

    // Step 2: Create intervals
    state = duckdb_query(conn.get(),
        "CREATE TEMP TABLE intervals AS "
        "SELECT start, \"end\" FROM ("
        "  SELECT "
        "    pos AS start, "
        "    LEAD(pos) OVER (ORDER BY pos) AS \"end\" "
        "  FROM boundaries"
        ") WHERE \"end\" IS NOT NULL;",
        &result);

    if (state == DuckDBError) {
        std::cerr << "Error creating intervals table: " << duckdb_result_error(&result) << std::endl;
        duckdb_destroy_result(&result);
        return 1;
    }
    duckdb_destroy_result(&result);
    std::cout << "✓ Created intervals table" << std::endl;

    // Debug: Test join directly
    state = duckdb_query(conn.get(),
        "SELECT i.start, i.\"end\", e.Inode, e.Path "
        "FROM intervals i "
        "LEFT JOIN extents e "
        "    ON e.PhysicalStart <= i.start "
        "    AND e.PhysicalEnd >= i.\"end\" "
        "ORDER BY i.start;",
        &result);
    if (state != DuckDBError) {
        uint64_t rowCount = duckdb_row_count(&result);
        std::cout << "DEBUG: Join test has " << rowCount << " rows:" << std::endl;
        for (uint64_t row = 0; row < std::min(rowCount, 5ULL); ++row) {
            uint64_t start = duckdb_value_uint64(&result, 0, row);
            uint64_t end = duckdb_value_uint64(&result, 1, row);
            uint64_t inode = duckdb_value_uint64(&result, 2, row);
            auto path = duckdb_value_varchar(&result, 3, row);
            std::cout << "  " << start << " - " << end << " => inode " << inode << " " << (path ? path : "NULL") << std::endl;
            if (path) duckdb_free(path);
        }
        duckdb_destroy_result(&result);
    }

    // Debug: Test aggregation with COUNT
    state = duckdb_query(conn.get(),
        "SELECT i.start, i.\"end\", COUNT(e.Path) as cnt "
        "FROM intervals i "
        "LEFT JOIN extents e "
        "    ON e.PhysicalStart <= i.start "
        "    AND e.PhysicalEnd >= i.\"end\" "
        "GROUP BY i.start, i.\"end\" "
        "ORDER BY i.start;",
        &result);
    if (state != DuckDBError) {
        uint64_t rowCount = duckdb_row_count(&result);
        std::cout << "DEBUG: COUNT aggregation test:" << std::endl;
        for (uint64_t row = 0; row < std::min(rowCount, 5ULL); ++row) {
            uint64_t start = duckdb_value_uint64(&result, 0, row);
            uint64_t end = duckdb_value_uint64(&result, 1, row);
            uint64_t cnt = duckdb_value_uint64(&result, 2, row);
            std::cout << "  " << start << " - " << end << " => count " << cnt << std::endl;
        }
        duckdb_destroy_result(&result);
    }

    // Step 3: Create diskmap - using string_agg for now as a workaround
    state = duckdb_query(conn.get(),
        "CREATE TABLE diskmap AS "
        "SELECT "
        "    i.start AS PhysicalStart, "
        "    i.\"end\" AS PhysicalEnd, "
        "    string_agg(e.Path, ',') AS Claimants "
        "FROM intervals i "
        "LEFT JOIN extents e "
        "    ON e.PhysicalStart <= i.start "
        "    AND e.PhysicalEnd >= i.\"end\" "
        "GROUP BY i.start, i.\"end\" "
        "ORDER BY i.start;",
        &result);

    if (state == DuckDBError) {
        std::cerr << "Error creating diskmap table: " << duckdb_result_error(&result) << std::endl;
        duckdb_destroy_result(&result);
        return 1;
    }
    duckdb_destroy_result(&result);
    std::cout << "✓ Created diskmap table" << std::endl;

    // Debug: Check diskmap contents
    state = duckdb_query(conn.get(), "SELECT * FROM diskmap LIMIT 5;", &result);
    if (state != DuckDBError) {
        uint64_t rowCount = duckdb_row_count(&result);
        std::cout << "DEBUG: Diskmap has " << rowCount << " rows" << std::endl;
        for (uint64_t row = 0; row < std::min(rowCount, 3ULL); ++row) {
            uint64_t start = duckdb_value_uint64(&result, 0, row);
            uint64_t end = duckdb_value_uint64(&result, 1, row);
            auto claimants = duckdb_value_varchar(&result, 2, row);
            std::cout << "  Row " << row << ": " << start << " - " << end << " | "
                      << (claimants ? claimants : "NULL") << std::endl;
            if (claimants) duckdb_free(claimants);
        }
        duckdb_destroy_result(&result);
    }

    // Generate HTML visualization
    std::string outputDir = "/tmp/diskmap-test";
    std::filesystem::remove_all(outputDir);
    std::filesystem::create_directories(outputDir);

    DiskMapHtmlGenerator generator(conn, 4096);
    if (!generator.generate(outputDir)) {
        std::cerr << "Failed to generate visualization" << std::endl;
        return 1;
    }
    std::cout << "✓ Generated HTML visualization" << std::endl;

    // Verify output files
    if (std::filesystem::exists(outputDir + "/index.html")) {
        std::cout << "✓ index.html created" << std::endl;
    }

    int chunkCount = 0;
    for (const auto& entry : std::filesystem::directory_iterator(outputDir)) {
        std::string filename = entry.path().filename().string();
        if (filename.compare(0, 6, "chunk_") == 0) {
            chunkCount++;
        }
    }
    std::cout << "✓ " << chunkCount << " chunk files created" << std::endl;

    std::cout << std::endl;
    std::cout << "SUCCESS: Disk map visualization generated at " << outputDir << std::endl;
    std::cout << "Open " << outputDir << "/index.html in a web browser to view" << std::endl;

    return 0;
}
