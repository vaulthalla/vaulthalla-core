#pragma once

#define FUSE_USE_VERSION 35

#include <memory>
#include <fuse3/fuse_lowlevel.h>

namespace vh::storage { class Manager; }
namespace vh::fs::cache { class Registry; }
namespace vh::vault { class APIKeyManager; }
namespace vh::auth { class AuthManager; }
namespace vh::shell { class UsageManager; }
namespace vh::stats::model { struct CacheStats; }

namespace vh::services {

class SyncController;

struct ServiceDepsRegistry {
    std::shared_ptr<storage::Manager> storageManager;
    std::shared_ptr<vault::APIKeyManager> apiKeyManager;
    std::shared_ptr<auth::AuthManager> authManager;
    std::shared_ptr<SyncController> syncController;
    std::shared_ptr<fs::cache::Registry> fsCache;
    std::shared_ptr<shell::UsageManager> shellUsageManager;
    fuse_session* fuseSession = nullptr;
    std::shared_ptr<stats::model::CacheStats> httpCacheStats;

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
