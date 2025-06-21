#pragma once

#include "types/db/StorageVolume.hpp"
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

class StorageManager {
  public:
    StorageManager();

    void initStorageEngines();

    void initUserStorage(const std::shared_ptr<vh::types::User>& user);
    void mountVault(std::shared_ptr<vh::types::Vault>&& vault);
    void addVault(std::shared_ptr<vh::types::Vault>&& vault);
    void removeVault(unsigned int vaultId);
    std::vector<std::shared_ptr<vh::types::Vault>> listVaults() const;
    std::shared_ptr<vh::types::Vault> getVault(unsigned int vaultId) const;

    void mountVolume(const std::shared_ptr<vh::types::StorageVolume>& volume,
                     std::shared_ptr<vh::types::Vault> vault = nullptr);
    void addVolume(std::shared_ptr<vh::types::StorageVolume> volume, unsigned int userId);
    void removeVolume(unsigned int volumeId, unsigned int userId);
    std::shared_ptr<vh::types::StorageVolume> getVolume(unsigned int volumeId, unsigned int userId) const;
    std::vector<std::shared_ptr<vh::types::StorageVolume>> listVolumes(unsigned int userId) const;

    std::shared_ptr<LocalDiskStorageEngine> getLocalEngine(unsigned short id) const;
    std::shared_ptr<CloudStorageEngine> getCloudEngine(unsigned short id) const;
    std::shared_ptr<StorageEngine> getEngine(unsigned short id) const;

    static bool pathsAreConflicting(const std::filesystem::path& path1, const std::filesystem::path& path2);

  private:
    mutable std::mutex mountsMutex_;
    std::unordered_map<unsigned int, std::shared_ptr<vh::types::Vault>> vaults_;
    std::unordered_map<unsigned int, std::shared_ptr<LocalDiskStorageEngine>> localEngines_;
    std::unordered_map<unsigned int, std::shared_ptr<CloudStorageEngine>> cloudEngines_;
};

} // namespace vh::storage
