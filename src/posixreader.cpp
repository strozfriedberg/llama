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
    // TODO: Implement in later task
    bufOut.clear();
    return false;
}

void PosixReader::walkFilesystem() {
    // TODO: Implement in later task
}

void PosixReader::handleEntry(const std::filesystem::directory_entry& entry) {
    // TODO: Implement in later task
}

#endif // __linux__
