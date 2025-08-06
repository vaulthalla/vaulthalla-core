#pragma once

#include "auth/AuthManager.hpp"
#include "storage/StorageManager.hpp"
#include "services/SyncController.hpp"
#include "storage/FSCache.hpp"
#include "keys/APIKeyManager.hpp"

#include <memory>

namespace vh::services {

struct ServiceDepsRegistry {
    std::shared_ptr<storage::StorageManager> storageManager;
    std::shared_ptr<keys::APIKeyManager> apiKeyManager;
    std::shared_ptr<auth::AuthManager> authManager;
    std::shared_ptr<SyncController> syncController;
    std::shared_ptr<storage::FSCache> fsCache;

    ServiceDepsRegistry(const ServiceDepsRegistry&) = delete;
    ServiceDepsRegistry& operator=(const ServiceDepsRegistry&) = delete;

    static ServiceDepsRegistry& instance() {
        static ServiceDepsRegistry instance_;
        return instance_;
    }

    static void init() {
        auto& ctx = instance();
        ctx.storageManager = std::make_shared<storage::StorageManager>();
        ctx.apiKeyManager = std::make_shared<keys::APIKeyManager>();
        ctx.authManager = std::make_shared<auth::AuthManager>();
        ctx.fsCache = std::make_shared<storage::FSCache>();
    }

    static void setSyncController(const std::shared_ptr<SyncController>& syncController) {
        instance().syncController = syncController;
    }

private:
    ServiceDepsRegistry() = default;  // private ctor
};

}
