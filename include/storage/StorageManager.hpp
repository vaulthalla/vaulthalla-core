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
struct User;
struct Vault;
struct Sync;
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

    void updatePaths(const fs::path& oldPath, const fs::path& newPath);

    fs::path resolvePathFromInode(fuse_ino_t ino);

    void initUserStorage(const std::shared_ptr<types::User>& user);

    std::shared_ptr<types::Vault> addVault(std::shared_ptr<types::Vault> vault,
                                           const std::shared_ptr<types::Sync>& sync = nullptr);

    void updateVault(const std::shared_ptr<types::Vault>& vault);

    void removeVault(unsigned int vaultId);

    std::vector<std::shared_ptr<types::Vault>> listVaults(const std::shared_ptr<types::User>& user) const;

    std::shared_ptr<types::Vault> getVault(unsigned int vaultId) const;

    void finishUpload(unsigned int vaultId, unsigned int userId, const std::filesystem::path& relPath) const;

    void removeEntry(unsigned int userId, unsigned int vaultId, const std::filesystem::path& relPath) const;

    [[nodiscard]] std::vector<std::shared_ptr<types::FSEntry>> listDir(unsigned int vaultId,
                                                                     const std::string& relPath,
                                                                     bool recursive = false) const;

    void mkdir(unsigned int vaultId, const std::string& relPath, const std::shared_ptr<types::User>& user) const;

    void move(unsigned int vaultId, unsigned int userId, const std::filesystem::path& from, const std::filesystem::path& to) const;

    void rename(unsigned int vaultId, unsigned int userId, const std::string& from, const std::string& to) const;

    void copy(unsigned int vaultId, unsigned int userId, const std::filesystem::path& from, const std::filesystem::path& to) const;

    static void syncNow(unsigned int vaultId) ;

    std::shared_ptr<StorageEngine> getEngine(unsigned int id) const;

private:
    mutable std::mutex mutex_;
    std::pmr::unordered_map<std::string, std::shared_ptr<StorageEngine>> engines_;
    std::unordered_map<unsigned int, std::shared_ptr<StorageEngine>> vaultToEngine_;

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
