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

class StorageManager {
public:
    StorageManager();

    void initStorageEngines();

    std::vector<std::shared_ptr<StorageEngine>> getEngines() const;

    std::shared_ptr<StorageEngine> resolveStorageEngine(const fs::path& absPath) const;

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
};

}
