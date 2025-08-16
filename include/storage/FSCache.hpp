#pragma once

#define FUSE_USE_VERSION 35

#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <filesystem>
#include <optional>
#include <fuse3/fuse_lowlevel.h>

namespace vh::types {
struct FSEntry;
}

namespace fs = std::filesystem;

namespace vh::storage {

struct CacheEntry {
    std::shared_ptr<types::FSEntry> entry;
    fs::path vaultPath;
    fs::path absPath;
    fuse_ino_t inode;
};

class FSCache {
public:
    FSCache();

    [[nodiscard]] std::shared_ptr<types::FSEntry> getEntry(const fs::path& absPath);

    fuse_ino_t assignInode(const fs::path& path);
    fuse_ino_t getOrAssignInode(const fs::path& path);
    fuse_ino_t resolveInode(const fs::path& absPath);
    fs::path resolvePath(fuse_ino_t ino);
    void linkPath(const fs::path& absPath, fuse_ino_t ino);
    void decrementInodeRef(fuse_ino_t ino, uint64_t nlookup);

    void cacheEntry(const std::shared_ptr<types::FSEntry>& entry);
    [[nodiscard]] bool entryExists(const fs::path& absPath) const;
    std::shared_ptr<types::FSEntry> getEntryFromInode(fuse_ino_t ino) const;
    void evictPath(const std::filesystem::path& path);

private:
    mutable std::shared_mutex mutex_;
    fuse_ino_t nextInode_ = 2;
    std::unordered_map<fuse_ino_t, fs::path> inodeToPath_;
    std::unordered_map<fs::path, fuse_ino_t> pathToInode_;
    std::unordered_map<fuse_ino_t, std::shared_ptr<types::FSEntry>> inodeToEntry_;
    std::pmr::unordered_map<fs::path, std::shared_ptr<types::FSEntry>> pathToEntry_;

    void initRoot();
    void restoreCache();
};

}
