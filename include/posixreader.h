// ABOUTME: Linux-only InputReader using POSIX APIs and FIEMAP
// ABOUTME: Extracts physical extent information for disk mapping

#pragma once

#ifdef __linux__

#include "inputreader.h"
#include "direntstack.h"
#include "extent.h"
#include "inode.h"
#include "recordhasher.h"
#include "duckextent.h"
#include "llamaduck.h"

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

    // Set database appender for extent storage
    void setExtentAppender(std::shared_ptr<LlamaDBAppender> appender);

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

    ExtentBatch ExtentsBatch;
    std::shared_ptr<LlamaDBAppender> ExtentAppender;
};

#endif // __linux__
