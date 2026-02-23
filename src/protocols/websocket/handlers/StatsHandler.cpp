#include "protocols/websocket/handlers/StatsHandler.hpp"

#include "concurrency/ThreadPoolManager.hpp"
#include "concurrency/ThreadPool.hpp"
#include "logging/LogRegistry.hpp"
#include "nlohmann/json.hpp"
#include "protocols/websocket/WebSocketSession.hpp"
#include "stats/VaultStatTask.hpp"
#include "types/stats/VaultStat.hpp"
#include "types/entities/User.hpp"
#include "../../../../include/types/stats/CacheStats.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "../../../../include/fs/cache/Registry.hpp"

#include <future>

using namespace vh::websocket;
using namespace vh::types;
using namespace vh::logging;
using namespace vh::stats;
using namespace vh::services;

json StatsHandler::vault(const json& payload, const WebSocketSession& session) {
    const auto& vaultId = payload.at("vault_id");
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageVault(vaultId))
        throw std::runtime_error("User does not have permission to manage this vault.");

    const auto task = std::make_shared<VaultStatTask>(vaultId);
    auto future = task->getFuture().value();
    concurrency::ThreadPoolManager::instance().statsPool()->submit(task);

    if (const auto stats = std::get<std::shared_ptr<VaultStat>>(future.get()))
        return {{"stats", stats}};

    throw std::runtime_error("Unable to load vault stats");
}

json StatsHandler::fsCache(const WebSocketSession& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->isAdmin())
        throw std::runtime_error("Must be an admin to view cache stats.");

    const auto stats = ServiceDepsRegistry::instance().fsCache->stats();
    if (!stats) throw std::runtime_error("No cache stats available.");
    return {{"stats", stats}};
}

json StatsHandler::httpCache(const WebSocketSession& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->isAdmin())
        throw std::runtime_error("Must be an admin to view cache stats.");

    return {{"stats", ServiceDepsRegistry::instance().httpCacheStats->snapshot()}};
}
