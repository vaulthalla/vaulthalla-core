#pragma once

#include "storage/LocalDiskStorageEngine.hpp"
#include "storage/CloudStorageEngine.hpp"
#include "types/Vault.hpp"

#include <memory>
#include <unordered_map>
#include <filesystem>
#include <mutex>
#include <string>

namespace vh::storage {

    class StorageManager {
    public:
        StorageManager();

        void initStorageEngines();

        void mount(std::unique_ptr<vh::types::Vault>&& vault);

        void addVault(std::unique_ptr<vh::types::Vault>&& vault);
        void removeVault(unsigned int vaultId);
        std::vector<std::unique_ptr<vh::types::Vault>> listVaults() const;
        std::unique_ptr<vh::types::Vault> getVault(unsigned int vaultId) const;

        std::shared_ptr<LocalDiskStorageEngine> getLocalEngine(unsigned short id) const;
        std::shared_ptr<CloudStorageEngine> getCloudEngine(unsigned short id) const;
        std::shared_ptr<StorageEngine> getEngine(unsigned short id) const;

    private:
        mutable std::mutex mountsMutex_;
        std::unordered_map<unsigned short, std::shared_ptr<LocalDiskStorageEngine>> localEngines_;
        std::unordered_map<unsigned short, std::shared_ptr<CloudStorageEngine>> cloudEngines_;
    };

} // namespace vh::storage
