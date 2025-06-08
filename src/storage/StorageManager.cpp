#include "storage/StorageManager.hpp"

#include <stdexcept>
#include <iostream>

namespace vh::storage {

    StorageManager::StorageManager() = default;

    void StorageManager::mountLocal(const std::string& mountName, const std::filesystem::path& rootPath) {
        std::lock_guard<std::mutex> lock(mountsMutex_);

        if (localEngines_.count(mountName)) {
            throw std::runtime_error("Local mount already exists: " + mountName);
        }

        localEngines_[mountName] = std::make_shared<LocalDiskStorageEngine>(rootPath);

        std::cout << "[StorageManager] Mounted local: " << mountName << " → " << rootPath << "\n";
    }

    void StorageManager::mountCloud(const std::string& mountName /* , cloud params */) {
        std::lock_guard<std::mutex> lock(mountsMutex_);

        if (cloudEngines_.count(mountName)) {
            throw std::runtime_error("Cloud mount already exists: " + mountName);
        }

        // For now stub → no cloud params yet
        cloudEngines_[mountName] = std::make_shared<CloudStorageEngine>();

        std::cout << "[StorageManager] Mounted cloud: " << mountName << "\n";
    }

    std::shared_ptr<LocalDiskStorageEngine> StorageManager::getLocalEngine(const std::string& mountName) const {
        std::lock_guard<std::mutex> lock(mountsMutex_);

        auto it = localEngines_.find(mountName);
        if (it == localEngines_.end()) {
            throw std::runtime_error("Unknown local mount: " + mountName);
        }

        return it->second;
    }

    std::shared_ptr<CloudStorageEngine> StorageManager::getCloudEngine(const std::string& mountName) const {
        std::lock_guard<std::mutex> lock(mountsMutex_);

        auto it = cloudEngines_.find(mountName);
        if (it == cloudEngines_.end()) {
            throw std::runtime_error("Unknown cloud mount: " + mountName);
        }

        return it->second;
    }

} // namespace vh::storage
