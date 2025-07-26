#pragma once

#define FUSE_USE_VERSION 35

#include <mutex>
#include <shared_mutex>
#include <vector>
#include <memory>
#include <filesystem>
#include <unordered_map>
#include <fuse3/fuse_lowlevel.h>

namespace vh::types {
struct FSEntry;
}

namespace fs = std::filesystem;

namespace vh::storage {

struct StorageEngine;

class StorageManager {
public:
    StorageManager();

    void initStorageEngines();

    std::vector<std::shared_ptr<types::FSEntry>> listDir(const fs::path& absPath, bool recursive = false) const;

    fuse_ino_t assignInode(const fs::path& path);

    fuse_ino_t getOrAssignInode(const fs::path& path);

    fs::path resolvePathFromInode(fuse_ino_t ino) const;

    fuse_ino_t resolveInode(const fs::path& absPath);

    void linkPath(const fs::path& absPath, fuse_ino_t ino);

    void decrementInodeRef(fuse_ino_t ino, uint64_t nlookup);

    std::vector<std::shared_ptr<StorageEngine>> getEngines() const;

    [[nodiscard]] char getPathType(const fs::path& absPath) const;

    [[nodiscard]] std::shared_ptr<types::FSEntry> getEntry(const fs::path& absPath);

    std::shared_ptr<StorageEngine> resolveStorageEngine(const fs::path& absPath) const;

    // -- Entry Cache Management --
    void cacheEntry(fuse_ino_t ino, std::shared_ptr<types::FSEntry> entry);
    [[nodiscard]] bool entryExists(const fs::path& absPath) const;
    std::shared_ptr<types::FSEntry> getEntryFromInode(fuse_ino_t ino) const;
    void evictEntry(fuse_ino_t ino);
    void evictPath(const std::filesystem::path& path);

    // -- Bidirectional Maintenance --
    void registerInode(fuse_ino_t ino, const std::filesystem::path& path, std::shared_ptr<types::FSEntry> entry);

    // -- Optional future UX --
    void updateCachedEntry(const std::shared_ptr<types::FSEntry>& entry);


private:
    mutable std::mutex mutex_;
    std::pmr::unordered_map<std::string, std::shared_ptr<StorageEngine>> engines_;

    fuse_ino_t nextInode_ = 2;
    std::unordered_map<fuse_ino_t, fs::path> inodeToPath_;
    std::unordered_map<fs::path, fuse_ino_t> pathToInode_;
    std::unordered_map<fuse_ino_t, std::shared_ptr<types::FSEntry>> inodeToEntry_;
    std::pmr::unordered_map<fs::path, std::shared_ptr<types::FSEntry>> pathToEntry_;
    mutable std::shared_mutex inodeMutex_;
};

}
