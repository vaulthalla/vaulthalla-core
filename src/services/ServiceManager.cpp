#include "services/ServiceManager.hpp"

namespace vh::services {
    ServiceManager::ServiceManager()
        : authManager_(std::make_shared<vh::auth::AuthManager>()),
          fsManager_(std::make_shared<vh::core::FSManager>()),
          searchIndex_(std::make_shared<vh::index::SearchIndex>()),
          storageManager_(std::make_shared<vh::storage::StorageManager>()),
          accessControl_(std::make_shared<vh::security::AccessControl>()),
          linkResolver_(std::make_shared<vh::share::LinkResolver>()) {}

    std::shared_ptr<vh::auth::AuthManager> ServiceManager::authManager() const { return authManager_; }
    std::shared_ptr<vh::core::FSManager> ServiceManager::fsManager() const { return fsManager_; }
    std::shared_ptr<vh::index::SearchIndex> ServiceManager::searchIndex() const { return searchIndex_; }
    std::shared_ptr<vh::storage::StorageManager> ServiceManager::storageManager() const { return storageManager_; }
    std::shared_ptr<vh::security::AccessControl> ServiceManager::accessControl() const { return accessControl_; }
    std::shared_ptr<vh::share::LinkResolver> ServiceManager::linkResolver() const { return linkResolver_; }
}