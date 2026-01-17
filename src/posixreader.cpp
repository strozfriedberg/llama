// ABOUTME: Linux-only InputReader using POSIX APIs and FIEMAP
// ABOUTME: Walks mounted filesystems extracting extent information

#ifdef __linux__

#include "posixreader.h"
#include "inputhandler.h"

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

void PosixReader::setExtentAppender(std::shared_ptr<LlamaDBAppender> appender) {
    ExtentAppender = appender;
}

bool PosixReader::startReading() {
    std::cerr << "[PosixReader] Starting filesystem walk of: " << Mountpoint << std::endl;
    std::cerr << "[PosixReader] ExtentAppender is " << (ExtentAppender ? "SET" : "NOT SET") << std::endl;

    walkFilesystem();

    // Flush remaining dirents
    while (!Dirents.empty()) {
        Input->push(Dirents.pop());
    }

    Input->flush();
    std::cerr << "[PosixReader] Filesystem walk complete" << std::endl;
    return true;
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

Inode PosixReader::statToInode(const struct stat& st, const std::string& path) {
    Inode inode;

    inode.Addr = st.st_ino;
    inode.FsOffset = 0;  // Set by caller if needed
    inode.Filesize = st.st_size;
    inode.Uid = st.st_uid;
    inode.Gid = st.st_gid;
    inode.NumLinks = st.st_nlink;
    inode.SeqNum = 0;

    // Determine type
    if (S_ISREG(st.st_mode)) {
        inode.Type = "file";
    } else if (S_ISDIR(st.st_mode)) {
        inode.Type = "directory";
    } else if (S_ISLNK(st.st_mode)) {
        inode.Type = "symlink";
    } else if (S_ISBLK(st.st_mode)) {
        inode.Type = "block";
    } else if (S_ISCHR(st.st_mode)) {
        inode.Type = "char";
    } else if (S_ISFIFO(st.st_mode)) {
        inode.Type = "fifo";
    } else if (S_ISSOCK(st.st_mode)) {
        inode.Type = "socket";
    } else {
        inode.Type = "unknown";
    }

    // Flags from mode
    inode.Flags = "";

    // Timestamps - convert to ISO8601 strings
    auto formatTime = [](time_t t) -> std::string {
        if (t == 0) return "";
        char buf[32];
        struct tm tm;
        gmtime_r(&t, &tm);
        strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
        return buf;
    };

    inode.Modified = formatTime(st.st_mtim.tv_sec);
    inode.Accessed = formatTime(st.st_atim.tv_sec);
    inode.Metadata = formatTime(st.st_ctim.tv_sec);
    inode.Created = "";  // POSIX doesn't have birth time

    inode.LinkTarget = "";  // Caller can fill if symlink
    inode.Id = "";  // Caller can generate hash

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
    extentsOut.clear();

    if (bufLen < sizeof(fiemap)) {
        return;
    }

    const fiemap* fm = reinterpret_cast<const fiemap*>(fiemapBuf);
    const size_t expectedSize = sizeof(fiemap) + fm->fm_mapped_extents * sizeof(fiemap_extent);

    if (bufLen < expectedSize) {
        return;
    }

    extentsOut.reserve(fm->fm_mapped_extents);

    for (uint32_t i = 0; i < fm->fm_mapped_extents; ++i) {
        const fiemap_extent& fe = fm->fm_extents[i];

        Extent ext;
        ext.LogicalStart = fe.fe_logical;
        ext.LogicalEnd = fe.fe_logical + fe.fe_length;
        ext.PhysicalStart = fe.fe_physical;
        ext.PhysicalEnd = fe.fe_physical + fe.fe_length;
        ext.Inode = inode;
        ext.FilesystemOffset = fsOffset;
        ext.Path = path;
        ext.Flags = flagsToString(fe.fe_flags);
        ext.Source = "filesystem";

        extentsOut.push_back(std::move(ext));
    }
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
    namespace fs = std::filesystem;

    static constexpr size_t EXTENT_BATCH_FLUSH_SIZE = 10000;

    std::vector<uint8_t> fiemapBuf;
    std::vector<Extent> extents;

    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(Mountpoint, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator();
         ++it)
    {
        if (ec) {
            ec.clear();
            continue;
        }

        handleEntry(*it);

        // For regular files, get FIEMAP data
        if (it->is_regular_file(ec) && !ec) {
            const std::string pathStr = it->path().string();
            int fd = open(pathStr.c_str(), O_RDONLY);
            if (fd >= 0) {
                struct stat st;
                if (fstat(fd, &st) == 0) {
                    if (callFiemap(fd, fiemapBuf)) {
                        parseExtents(fiemapBuf.data(), fiemapBuf.size(),
                                     st.st_ino, pathStr, FsOffset, extents);

                        std::cerr << "[PosixReader] File: " << pathStr << " has " << extents.size() << " extents" << std::endl;

                        // Add extents to batch and flush when needed
                        for (const auto& ext : extents) {
                            ExtentsBatch.add(ext);
                            if (ExtentsBatch.size() >= EXTENT_BATCH_FLUSH_SIZE) {
                                if (ExtentAppender) {
                                    ExtentsBatch.copyToDB(ExtentAppender->get());
                                }
                                ExtentsBatch.clear();
                            }
                        }
                    } else {
                        std::cerr << "[PosixReader] FIEMAP failed for: " << pathStr << std::endl;
                    }
                }
                close(fd);
            }
        }
    }

    // Flush remaining extents
    if (ExtentsBatch.size() > 0 && ExtentAppender) {
        std::cerr << "[PosixReader] Flushing " << ExtentsBatch.size() << " remaining extents" << std::endl;
        ExtentsBatch.copyToDB(ExtentAppender->get());
        ExtentsBatch.clear();
    } else if (ExtentsBatch.size() > 0) {
        std::cerr << "[PosixReader] WARNING: " << ExtentsBatch.size() << " extents NOT flushed (no appender)" << std::endl;
    }
}

void PosixReader::handleEntry(const std::filesystem::directory_entry& entry) {
    std::error_code ec;
    struct stat st;

    if (lstat(entry.path().c_str(), &st) != 0) {
        return;
    }

    Inode inode = statToInode(st, entry.path().string());
    Input->push(inode);

    // TODO: Handle dirents similar to TskReader/DirReader
}

#endif // __linux__
