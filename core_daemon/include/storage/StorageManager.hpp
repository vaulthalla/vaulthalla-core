#pragma once

#include "types/db/Volume.hpp"
#include "types/db/Vault.hpp"
#include "storage/CloudStorageEngine.hpp"
#include "storage/LocalDiskStorageEngine.hpp"
#include <filesystem>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace vh::types {
class User;
}

namespace vh::storage {

struct UploadContext {
    unsigned int vaultId;
    unsigned int volumeId;
    std::filesystem::path relPath;
    std::shared_ptr<types::User> user;
};

class StorageManager {
public:
    StorageManager();

    void initStorageEngines();

    void initUserStorage(const std::shared_ptr<types::User>& user);

    void addVault(std::shared_ptr<types::Vault>&& vault);

    void removeVault(unsigned int vaultId);

    std::vector<std::shared_ptr<types::Vault> > listVaults(const std::shared_ptr<types::User>& user) const;

    std::shared_ptr<types::Vault> getVault(unsigned int vaultId) const;

    void finishUpload(unsigned int vaultId, unsigned int volumeId, const std::filesystem::path& relPath,
                      const std::shared_ptr<types::User>& user) const;

    [[nodiscard]] std::vector<std::shared_ptr<types::File> > listDir(unsigned int vaultId, unsigned int volumeId,
                                                                     const std::string& relPath,
                                                                     bool recursive = false) const;

    void mkdir(unsigned int vaultId, unsigned int volumeId, const std::string& relPath,
               const std::shared_ptr<types::User>& user) const;

    void mountVolume(const std::shared_ptr<types::Volume>& volume);

    void addVolume(std::shared_ptr<types::Volume> volume, unsigned int userId);

    void removeVolume(unsigned int volumeId, unsigned int userId);

    std::shared_ptr<types::Volume> getVolume(unsigned int volumeId, unsigned int userId) const;

    std::vector<std::shared_ptr<types::Volume> > listVolumes(unsigned int userId) const;

    std::shared_ptr<LocalDiskStorageEngine> getLocalEngine(unsigned int id) const;

    std::shared_ptr<CloudStorageEngine> getCloudEngine(unsigned int id) const;

    std::shared_ptr<StorageEngine> getEngine(unsigned int id) const;

    static bool pathsAreConflicting(const std::filesystem::path& path1, const std::filesystem::path& path2);

    static bool hasLogicalParent(const std::filesystem::path& relPath);

private:
    mutable std::mutex mountsMutex_;
    std::unordered_map<unsigned int, std::shared_ptr<StorageEngine> > engines_;
};

} // namespace vh::storage