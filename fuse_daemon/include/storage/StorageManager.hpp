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

struct PendingRename {
    std::filesystem::path oldPath;
    std::filesystem::path newPath;
};

class StorageManager {
public:
    StorageManager();

    void initStorageEngines();

    std::vector<std::shared_ptr<types::FSEntry>> listDir(const fs::path& absPath, bool recursive = false) const;

    void registerEntry(unsigned int entryId);

    fuse_ino_t assignInode(const fs::path& path);

    fuse_ino_t getOrAssignInode(const fs::path& path);

    fuse_ino_t resolveInode(const fs::path& absPath);

    void linkPath(const fs::path& absPath, fuse_ino_t ino);

    void decrementInodeRef(fuse_ino_t ino, uint64_t nlookup);

    std::vector<std::shared_ptr<StorageEngine>> getEngines() const;

    [[nodiscard]] char getPathType(const fs::path& absPath) const;

    [[nodiscard]] std::shared_ptr<types::FSEntry> getEntry(const fs::path& absPath);

    std::shared_ptr<StorageEngine> resolveStorageEngine(const fs::path& absPath) const;

    // -- Entry Cache Management --
    void cacheEntry(const std::shared_ptr<types::FSEntry>& entry);
    [[nodiscard]] bool entryExists(const fs::path& absPath) const;
    std::shared_ptr<types::FSEntry> getEntryFromInode(fuse_ino_t ino) const;
    void evictEntry(fuse_ino_t ino);
    void evictPath(const std::filesystem::path& path);

    // -- Optional future UX --
    void updateCachedEntry(const std::shared_ptr<types::FSEntry>& entry);

    std::shared_ptr<types::FSEntry> createFile(const fs::path& path, mode_t mode, uid_t uid, gid_t gid);

    void renamePath(const fs::path& oldPath, const fs::path& newPath);

    void queuePendingRename(fuse_ino_t ino, const fs::path& oldPath, const fs::path& newPath);

    std::optional<PendingRename> getPendingRename(fuse_ino_t ino) const;

    void updatePaths(const fs::path& oldPath, const fs::path& newPath, const std::optional<std::string>& iv_b64 = std::nullopt);

    fs::path resolvePathFromInode(fuse_ino_t ino);

private:
    mutable std::mutex mutex_;
    std::pmr::unordered_map<std::string, std::shared_ptr<StorageEngine>> engines_;

    fuse_ino_t nextInode_ = 2;
    std::unordered_map<fuse_ino_t, fs::path> inodeToPath_;
    std::unordered_map<fs::path, fuse_ino_t> pathToInode_;
    std::unordered_map<fuse_ino_t, std::shared_ptr<types::FSEntry>> inodeToEntry_;
    std::pmr::unordered_map<fs::path, std::shared_ptr<types::FSEntry>> pathToEntry_;
    mutable std::shared_mutex inodeMutex_;

    mutable std::mutex renameQueueMutex_;
    std::unordered_map<fuse_ino_t, PendingRename> renameRequests_;
};

}
