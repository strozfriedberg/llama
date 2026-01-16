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

void PosixReader::walkFilesystem() {
    // TODO: Implement in later task
}

void PosixReader::handleEntry(const std::filesystem::directory_entry& entry) {
    // TODO: Implement in later task
}

#endif // __linux__
