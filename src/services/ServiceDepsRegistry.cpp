#include "services/ServiceDepsRegistry.hpp"
#include "auth/AuthManager.hpp"
#include "storage/Manager.hpp"
#include "services/SyncController.hpp"
#include "fs/cache/Registry.hpp"
#include "vault/APIKeyManager.hpp"
#include "logging/LogRegistry.hpp"
#include "stats/model/CacheStats.hpp"
#include "usage/include/UsageManager.hpp"

using namespace vh::services;
using namespace vh::storage;
using namespace vh::crypto;
using namespace vh::auth;
using namespace vh::logging;
using namespace vh::stats::model;
using namespace vh::vault;
using namespace vh::fs::cache;

ServiceDepsRegistry& ServiceDepsRegistry::instance() {
    static ServiceDepsRegistry instance_;
    return instance_;
}

void ServiceDepsRegistry::init() {
    if (const auto& registry = instance();
        registry.storageManager || registry.apiKeyManager || registry.authManager || registry.fsCache
        || registry.syncController || registry.shellUsageManager) {
        LogRegistry::vaulthalla()->warn("[ServiceDepsRegistry] Already initialized, ignoring second init()");
        return;
    }

    LogRegistry::vaulthalla()->info("[ServiceDepsRegistry] Initializing...");

    auto& ctx = instance();
    ctx.storageManager = std::make_shared<Manager>();
    ctx.apiKeyManager = std::make_shared<APIKeyManager>();
    ctx.authManager = std::make_shared<AuthManager>();
    ctx.fsCache = std::make_shared<Registry>();
    ctx.shellUsageManager = std::make_shared<shell::UsageManager>();
    ctx.httpCacheStats = std::make_shared<CacheStats>();

    LogRegistry::vaulthalla()->info("[ServiceDepsRegistry] Initialized.");
}

void ServiceDepsRegistry::setSyncController(const std::shared_ptr<SyncController>& syncController) {
    instance().syncController = syncController;
}

