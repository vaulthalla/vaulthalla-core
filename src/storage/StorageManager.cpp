#include "storage/StorageManager.hpp"
#include "database/Queries/VaultQueries.hpp"
#include "storage/LocalDiskStorageEngine.hpp"
#include "types/Vault.hpp"
#include "types/User.hpp"
#include "../../shared/include/types/StorageVolume.hpp"

#include <stdexcept>
#include <iostream>

namespace vh::storage {

    StorageManager::StorageManager() { initStorageEngines(); }

    void StorageManager::initStorageEngines() {
        std::lock_guard<std::mutex> lock(mountsMutex_);
        try {
            for (auto& vault : vh::database::VaultQueries::listVaults())
                vaults_[vault->id] = std::move(vault);
        } catch (const std::exception& e) {
            std::cerr << "[StorageManager] Error initializing storage engines: " << e.what() << "\n";
            throw;
        }
    }

    void StorageManager::initUserStorage(const std::shared_ptr<vh::types::User>& user) {
        try {
            std::cout << "[StorageManager] Initializing storage for user: " << user->email << "\n";

            if (!user->id) throw std::runtime_error("User ID is not set. Cannot initialize storage.");

            std::shared_ptr<vh::types::Vault> vault = std::make_shared<vh::types::LocalDiskVault>(user->name + "'s Local Disk Vault",
                                                                                                  std::filesystem::path(std::getenv("VAULTHALLA_ROOT_DIR")) / "users" / user->email);
            {
                std::lock_guard<std::mutex> lock(mountsMutex_);
                vault->id = vh::database::VaultQueries::addVault(vault);
                vault = vh::database::VaultQueries::getVault(vault->id);
            }

            if (!vault) throw std::runtime_error("Failed to create or retrieve vault for user: " + user->email);

            vaults_[vault->id] = std::static_pointer_cast<types::LocalDiskVault>(vault);

            auto volume = std::make_shared<vh::types::StorageVolume>(vault->id,
                                                                     user->name + " Default Volume",
                                                                     std::filesystem::path(user->email + "_default_volume"));

            {
                std::lock_guard<std::mutex> lock(mountsMutex_);
                volume->id = vh::database::VaultQueries::addVolume(user->id, volume);
                volume = vh::database::VaultQueries::getVolume(volume->id);
            }

            if (!volume) throw std::runtime_error("Failed to initialize user storage: Volume not found after creation");

            mountVolume(volume, vault);

            std::cout << "[StorageManager] Initialized storage for user: " << user->email << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[StorageManager] Error initializing user storage: " << e.what() << "\n";
            throw;
        }
    }

    void StorageManager::addVault(std::shared_ptr<vh::types::Vault>&& vault) {
        if (!vault) throw std::invalid_argument("Vault cannot be null");
        std::lock_guard<std::mutex> lock(mountsMutex_);

        vh::database::VaultQueries::addVault(vault);
        vault = vh::database::VaultQueries::getVault(vault->id);
        vaults_[vault->id] = std::move(vault);
    }

    void StorageManager::removeVault(unsigned int vaultId) {
        std::lock_guard<std::mutex> lock(mountsMutex_);
        vh::database::VaultQueries::removeVault(vaultId);

        localEngines_.erase(vaultId);
        cloudEngines_.erase(vaultId);
        std::cout << "[StorageManager] Removed vault with ID: " << vaultId << "\n";
    }

    std::vector<std::shared_ptr<vh::types::Vault>> StorageManager::listVaults() const {
        std::lock_guard<std::mutex> lock(mountsMutex_);
        return vh::database::VaultQueries::listVaults();
    }

    std::shared_ptr<vh::types::Vault> StorageManager::getVault(unsigned int vaultId) const {
        std::lock_guard<std::mutex> lock(mountsMutex_);
        if (vaults_.find(vaultId) != vaults_.end()) return vaults_.at(vaultId);
        return vh::database::VaultQueries::getVault(vaultId);
    }

    void StorageManager::mountVolume(const std::shared_ptr<vh::types::StorageVolume> &volume,
                                     std::shared_ptr<vh::types::Vault> vault) {
        if (!volume) throw std::invalid_argument("Volume cannot be null");
        std::lock_guard<std::mutex> lock(mountsMutex_);

        if (!vault) vault = vh::database::VaultQueries::getVault(volume->vault_id);
        if (!vault) throw std::runtime_error("Vault not found for volume ID: " + std::to_string(volume->vault_id));

        auto volumesOnVault = vh::database::VaultQueries::listVaultVolumes(vault->id);

        if (auto* vaultDisk = dynamic_cast<vh::types::LocalDiskVault*>(&*vault)) {
            auto newVolPath = vaultDisk->mount_point / volume->path_prefix;
            for (const auto& existingVolume : volumesOnVault) {
                if (existingVolume->id == volume->id) continue;
                if (pathsAreConflicting(vaultDisk->mount_point / existingVolume->path_prefix, newVolPath)) {
                    throw std::runtime_error("Volume path conflicts with existing volume: " + existingVolume->path_prefix.string());
                }
            }
            localEngines_[volume->id] = std::make_shared<LocalDiskStorageEngine>(newVolPath);
        } else if (auto* vaultS3 = dynamic_cast<vh::types::S3Vault*>(vault.get())) {
            // TODO: Handle S3 volume path conflicts
        }
    }

    void StorageManager::addVolume(std::shared_ptr<vh::types::StorageVolume> volume, unsigned int userId) {
        if (!volume) throw std::invalid_argument("Volume cannot be null");

        {
            std::lock_guard<std::mutex> lock(mountsMutex_);
            volume->id = vh::database::VaultQueries::addVolume(userId, volume);
            volume = vh::database::VaultQueries::getVolume(volume->id);
        }

        mountVolume(volume);
        std::cout << "[StorageManager] Added volume: " << volume->name << " with ID: " << volume->id << "\n";
    }

    void StorageManager::removeVolume(unsigned int volumeId, unsigned int userId) {
        std::lock_guard<std::mutex> lock(mountsMutex_);

        auto volume = vh::database::VaultQueries::getVolume(volumeId);
        if (!volume) throw std::runtime_error("Volume not found with ID: " + std::to_string(volumeId));

        try { vh::database::VaultQueries::getUserVolume(volumeId, userId); }
        catch (const std::runtime_error& e) {
            throw std::runtime_error("User does not have access to volume ID: " + std::to_string(volumeId));
        }

        vh::database::VaultQueries::removeVolume(volumeId);

        localEngines_.erase(volumeId);
        cloudEngines_.erase(volumeId);
        std::cout << "[StorageManager] Removed volume with ID: " << volumeId << "\n";
    }

    std::shared_ptr<vh::types::StorageVolume> StorageManager::getVolume(unsigned int volumeId, unsigned int userId) const {
        std::lock_guard<std::mutex> lock(mountsMutex_);
        auto volume = vh::database::VaultQueries::getVolume(volumeId);
        if (!volume) throw std::runtime_error("Volume not found with ID: " + std::to_string(volumeId));

        try {
            vh::database::VaultQueries::getUserVolume(volumeId, userId);
        } catch (const std::runtime_error& e) {
            throw std::runtime_error("User does not have access to volume ID: " + std::to_string(volumeId));
        }

        return volume;
    }

    std::vector<std::shared_ptr<vh::types::StorageVolume>> StorageManager::listVolumes(unsigned int userId) const {
        std::lock_guard<std::mutex> lock(mountsMutex_);
        return vh::database::VaultQueries::listUserVolumes(userId);
    }

    std::shared_ptr<LocalDiskStorageEngine> StorageManager::getLocalEngine(unsigned short id) const {
        std::lock_guard<std::mutex> lock(mountsMutex_);
        auto it = localEngines_.find(id);
        if (it != localEngines_.end()) return it->second;
        return nullptr;
    }

    std::shared_ptr<CloudStorageEngine> StorageManager::getCloudEngine(unsigned short id) const {
        std::lock_guard<std::mutex> lock(mountsMutex_);
        auto it = cloudEngines_.find(id);
        if (it != cloudEngines_.end()) return it->second;
        return nullptr;
    }

    std::shared_ptr<StorageEngine> StorageManager::getEngine(unsigned short id) const {
        if (auto localEngine = getLocalEngine(id)) return localEngine;
        if (auto cloudEngine = getCloudEngine(id)) return cloudEngine;
        return nullptr; // No engine found for the given ID
    }

    bool StorageManager::pathsAreConflicting(const std::filesystem::path& path1, const std::filesystem::path& path2) {
        std::error_code ec;

        auto weak1 = std::filesystem::weakly_canonical(path1, ec);
        if (ec) weak1 = std::filesystem::absolute(path1);  // fallback if partial canonical fails

        ec.clear();
        auto weak2 = std::filesystem::weakly_canonical(path2, ec);
        if (ec) weak2 = std::filesystem::absolute(path2);

        return weak1 == weak2;
    }

} // namespace vh::storage
