#pragma once

#include <memory>

#include "storage/FSCache.hpp"

namespace vh::storage {
    class StorageManager;
    class FSCache;
}

namespace vh::crypto {
    class APIKeyManager;
}

namespace vh::auth {
    class AuthManager;
}

namespace vh::shell {
    class UsageManager;
}

namespace vh::services {

class SyncController;

struct ServiceDepsRegistry {
    std::shared_ptr<storage::StorageManager> storageManager;
    std::shared_ptr<crypto::APIKeyManager> apiKeyManager;
    std::shared_ptr<auth::AuthManager> authManager;
    std::shared_ptr<SyncController> syncController;
    std::shared_ptr<storage::FSCache> fsCache;
    std::shared_ptr<shell::UsageManager> shellUsageManager;
    fuse_session* fuseSession = nullptr;

    void setFuseSession(fuse_session* fuseSession) { this->fuseSession = fuseSession; }

    ServiceDepsRegistry(const ServiceDepsRegistry&) = delete;
    ServiceDepsRegistry& operator=(const ServiceDepsRegistry&) = delete;

    static ServiceDepsRegistry& instance();

    static void init();

    static void setSyncController(const std::shared_ptr<SyncController>& syncController);

private:
    ServiceDepsRegistry() = default;  // private ctor
};

}
