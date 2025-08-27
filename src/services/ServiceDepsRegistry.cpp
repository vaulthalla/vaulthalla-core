#include "services/ServiceDepsRegistry.hpp"
#include "auth/AuthManager.hpp"
#include "storage/StorageManager.hpp"
#include "services/SyncController.hpp"
#include "storage/FSCache.hpp"
#include "crypto/APIKeyManager.hpp"
#include "services/LogRegistry.hpp"

using namespace vh::services;
using namespace vh::storage;
using namespace vh::crypto;
using namespace vh::auth;
using namespace vh::logging;

ServiceDepsRegistry& ServiceDepsRegistry::instance() {
    static ServiceDepsRegistry instance_;
    return instance_;
}


void ServiceDepsRegistry::init() {
    if (const auto& registry = instance();
        registry.storageManager || registry.apiKeyManager || registry.authManager || registry.fsCache) {
        LogRegistry::vaulthalla()->warn("[ServiceDepsRegistry] Already initialized, ignoring second init()");
        return;
    }

    LogRegistry::vaulthalla()->info("[ServiceDepsRegistry] Initializing...");

    auto& ctx = instance();
    ctx.storageManager = std::make_shared<StorageManager>();
    ctx.apiKeyManager = std::make_shared<APIKeyManager>();
    ctx.authManager = std::make_shared<AuthManager>();
    ctx.fsCache = std::make_shared<FSCache>();

    LogRegistry::vaulthalla()->info("[ServiceDepsRegistry] Initialized.");
}

void ServiceDepsRegistry::setSyncController(const std::shared_ptr<SyncController>& syncController) {
    instance().syncController = syncController;
}

