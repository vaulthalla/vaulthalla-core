#include "services/ServiceManager.hpp"

namespace vh::services {
    ServiceManager::ServiceManager()
        : storageManager_(std::make_shared<vh::storage::StorageManager>()),
          authManager_(std::make_shared<vh::auth::AuthManager>(storageManager_)),
          searchIndex_(std::make_shared<vh::index::SearchIndex>()),
          linkResolver_(std::make_shared<vh::share::LinkResolver>()) {}

    std::shared_ptr<vh::auth::AuthManager> ServiceManager::authManager() const { return authManager_; }
    std::shared_ptr<vh::index::SearchIndex> ServiceManager::searchIndex() const { return searchIndex_; }
    std::shared_ptr<vh::storage::StorageManager> ServiceManager::storageManager() const { return storageManager_; }
    std::shared_ptr<vh::share::LinkResolver> ServiceManager::linkResolver() const { return linkResolver_; }
}