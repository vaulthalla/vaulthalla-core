#include "storage/StorageManager.hpp"
#include "database/Queries/VaultQueries.hpp"

#include <stdexcept>
#include <iostream>

namespace vh::storage {

    StorageManager::StorageManager() { initStorageEngines(); }

    void StorageManager::initStorageEngines() {
        std::lock_guard<std::mutex> lock(mountsMutex_);
        for (auto& vault : vh::database::VaultQueries::listVaults()) mount(std::move(vault));
    }

    void StorageManager::mount(std::unique_ptr<vh::types::Vault> &&vault) {
        if (auto* diskVault = dynamic_cast<vh::types::LocalDiskVault*>(vault.get())) {
            auto localEngine = std::make_shared<LocalDiskStorageEngine>(diskVault->mount_point);
            localEngines_[diskVault->id] = localEngine;
            std::cout << "[StorageManager] Mounted local disk vault: " << diskVault->name << "\n";
        } else if (auto* cloudVault = dynamic_cast<vh::types::S3Vault*>(vault.get())) {
            // TODO: Initialize cloud storage engine with S3 credentials
            auto cloudEngine = std::make_shared<CloudStorageEngine>();
            cloudEngines_[cloudVault->id] = cloudEngine;
            std::cout << "[StorageManager] Mounted cloud vault: " << cloudVault->name << "\n";
        } else {
            throw std::runtime_error("Unsupported vault type: " + std::to_string(static_cast<int>(vault->type)));
        }
    }

    void StorageManager::addVault(std::unique_ptr<vh::types::Vault>&& vault) {
        if (!vault) throw std::invalid_argument("Vault cannot be null");
        std::lock_guard<std::mutex> lock(mountsMutex_);

        vh::database::VaultQueries::addVault(*vault);
        vault = vh::database::VaultQueries::getVault(vault->id);
        mount(std::move(vault));
    }

    void StorageManager::removeVault(unsigned int vaultId) {
        std::lock_guard<std::mutex> lock(mountsMutex_);
        vh::database::VaultQueries::removeVault(vaultId);

        localEngines_.erase(vaultId);
        cloudEngines_.erase(vaultId);
        std::cout << "[StorageManager] Removed vault with ID: " << vaultId << "\n";
    }

    std::vector<std::unique_ptr<vh::types::Vault>> StorageManager::listVaults() const {
        std::lock_guard<std::mutex> lock(mountsMutex_);
        return vh::database::VaultQueries::listVaults();
    }

    std::unique_ptr<vh::types::Vault> StorageManager::getVault(unsigned int vaultId) const {
        std::lock_guard<std::mutex> lock(mountsMutex_);
        return vh::database::VaultQueries::getVault(vaultId);
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

} // namespace vh::storage
