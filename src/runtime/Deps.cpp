#include "runtime/Deps.hpp"
#include "auth/Manager.hpp"
#include "storage/Manager.hpp"
#include "sync/Controller.hpp"
#include "fs/cache/Registry.hpp"
#include "vault/APIKeyManager.hpp"
#include "log/Registry.hpp"
#include "stats/model/CacheStats.hpp"
#include "UsageManager.hpp"
#include "crypto/secrets/Manager.hpp"

using namespace vh::runtime;

Deps& Deps::get() {
    static Deps instance_;
    return instance_;
}

void Deps::init() {
    if (const auto& registry = get();
        registry.storageManager || registry.apiKeyManager || registry.authManager || registry.sessionManager
        || registry.secretsManager || registry.fsCache || registry.syncController || registry.shellUsageManager) {
        return;
    }

    auto& ctx = get();
    ctx.storageManager = std::make_shared<storage::Manager>();
    ctx.apiKeyManager = std::make_shared<vault::APIKeyManager>();
    ctx.authManager = std::make_shared<auth::Manager>();
    ctx.sessionManager = std::make_shared<auth::session::Manager>();
    ctx.secretsManager = std::make_shared<crypto::secrets::Manager>();
    ctx.fsCache = std::make_shared<fs::cache::Registry>();
    ctx.shellUsageManager = std::make_shared<protocols::shell::UsageManager>();
    ctx.httpCacheStats = std::make_shared<stats::model::CacheStats>();
}

void Deps::setSyncController(const std::shared_ptr<sync::Controller>& syncController) {
    get().syncController = syncController;
}
