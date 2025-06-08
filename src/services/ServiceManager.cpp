#include "services/ServiceManager.hpp"

namespace vh::services {
    ServiceManager::ServiceManager(const std::filesystem::path& configPath)
        : configPath_(configPath) {
        loadConfig();
    }

    std::shared_ptr<vh::auth::AuthManager> ServiceManager::authManager() const { return authManager_; }
    std::shared_ptr<vh::core::FSManager> ServiceManager::fsManager() const { return fsManager_; }
    std::shared_ptr<vh::index::SearchIndex> ServiceManager::searchIndex() const { return searchIndex_; }
    std::shared_ptr<vh::storage::StorageManager> ServiceManager::storageManager() const { return storageManager_; }
    std::shared_ptr<vh::security::AccessControl> ServiceManager::accessControl() const { return accessControl_; }
    std::shared_ptr<vh::share::LinkResolver> ServiceManager::linkResolver() const { return linkResolver_; }

    void ServiceManager::loadConfig() {
        // TODO
        // Load configuration from the specified path
        // This is a placeholder for actual configuration loading logic
        // In a real implementation, you would read from a file or database
    }
}