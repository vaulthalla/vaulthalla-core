#pragma once

#include "auth/AuthManager.hpp"
#include "storage/StorageManager.hpp"
#include "services/SyncController.hpp"

#include <memory>
#include <mutex>

namespace vh::services {

struct ServiceDepsRegistry {
    std::shared_ptr<storage::StorageManager> storageManager;
    std::shared_ptr<auth::AuthManager> authManager;
    std::shared_ptr<SyncController> syncController;

    ServiceDepsRegistry(const ServiceDepsRegistry&) = delete;
    ServiceDepsRegistry& operator=(const ServiceDepsRegistry&) = delete;

    static ServiceDepsRegistry& instance() {
        static ServiceDepsRegistry instance_;
        return instance_;
    }

    static void init() {
        auto& ctx = instance();
        ctx.storageManager = std::make_shared<storage::StorageManager>();
        ctx.authManager = std::make_shared<auth::AuthManager>();
    }

    static void setSyncController(const std::shared_ptr<SyncController>& syncController) {
        instance().syncController = syncController;
    }

private:
    ServiceDepsRegistry() = default;  // private ctor
};

}
