#include "services/ServiceManager.hpp"

namespace vh::services {
ServiceManager::ServiceManager()
    : storageManager_(std::make_shared<storage::StorageManager>()),
      authManager_(std::make_shared<auth::AuthManager>(storageManager_)),
      searchIndex_(std::make_shared<index::SearchIndex>()),
      linkResolver_(std::make_shared<share::LinkResolver>()) {}

std::shared_ptr<auth::AuthManager> ServiceManager::authManager() const {
    return authManager_;
}
std::shared_ptr<index::SearchIndex> ServiceManager::searchIndex() const {
    return searchIndex_;
}
std::shared_ptr<storage::StorageManager> ServiceManager::storageManager() const {
    return storageManager_;
}
std::shared_ptr<share::LinkResolver> ServiceManager::linkResolver() const {
    return linkResolver_;
}
} // namespace vh::services