#include "services/ServiceManager.hpp"

namespace vh::services {

ServiceManager::ServiceManager()
    : storageManager_(std::make_shared<storage::StorageManager>()),
      authManager_(std::make_shared<auth::AuthManager>(storageManager_)) {}

std::shared_ptr<auth::AuthManager> ServiceManager::authManager() const {
    return authManager_;
}

std::shared_ptr<storage::StorageManager> ServiceManager::storageManager() const {
    return storageManager_;
}

} // namespace vh::services