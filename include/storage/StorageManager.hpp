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

    std::vector<std::shared_ptr<StorageEngine>> getEngines() const;

    [[nodiscard]] char getPathType(const fs::path& absPath) const;

    std::shared_ptr<StorageEngine> resolveStorageEngine(const fs::path& absPath) const;


    void queuePendingRename(fuse_ino_t ino, const fs::path& oldPath, const fs::path& newPath);

    void clearPendingRename(fuse_ino_t ino);

    std::optional<PendingRename> getPendingRename(fuse_ino_t ino) const;

    void initUserStorage(const std::shared_ptr<types::User>& user);

    std::shared_ptr<types::Vault> addVault(std::shared_ptr<types::Vault> vault,
                                           const std::shared_ptr<types::Sync>& sync = nullptr);

    void updateVault(const std::shared_ptr<types::Vault>& vault);

    void removeVault(unsigned int vaultId);

    std::shared_ptr<types::Vault> getVault(unsigned int vaultId) const;

    std::shared_ptr<StorageEngine> getEngine(unsigned int id) const;

private:
    mutable std::mutex mutex_;
    std::pmr::unordered_map<std::string, std::shared_ptr<StorageEngine>> engines_;
    std::unordered_map<unsigned int, std::shared_ptr<StorageEngine>> vaultToEngine_;

    mutable std::mutex renameQueueMutex_;
    std::unordered_map<fuse_ino_t, PendingRename> renameRequests_;
};

}
