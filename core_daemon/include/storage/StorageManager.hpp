#pragma once

#include <filesystem>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace vh::types {
struct User;
struct FSEntry;
struct Sync;
struct Vault;
}

namespace vh::services {
class SyncController;
}

namespace vh::storage {

struct StorageEngine;

class StorageManager : public std::enable_shared_from_this<StorageManager> {
public:
    StorageManager();

    void initStorageEngines();

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
    mutable std::mutex mountsMutex_;
    std::unordered_map<unsigned int, std::shared_ptr<StorageEngine>> engines_;
};

} // namespace vh::storage