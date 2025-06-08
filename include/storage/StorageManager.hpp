#pragma once

#include "storage/LocalDiskStorageEngine.hpp"
#include "storage/CloudStorageEngine.hpp"

#include <memory>
#include <unordered_map>
#include <filesystem>
#include <mutex>
#include <string>

namespace vh::storage {

    class StorageManager {
    public:
        StorageManager();

        void mountLocal(const std::string& mountName, const std::filesystem::path& rootPath);
        void mountCloud(const std::string& mountName /* , cloud params */);

        std::shared_ptr<LocalDiskStorageEngine> getLocalEngine(const std::string& mountName) const;
        std::shared_ptr<CloudStorageEngine> getCloudEngine(const std::string& mountName) const;

    private:
        mutable std::mutex mountsMutex_;
        std::unordered_map<std::string, std::shared_ptr<LocalDiskStorageEngine>> localEngines_;
        std::unordered_map<std::string, std::shared_ptr<CloudStorageEngine>> cloudEngines_;
    };

} // namespace vh::storage
