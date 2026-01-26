#include "protocols/websocket/handlers/StatsHandler.hpp"

#include "concurrency/ThreadPoolManager.hpp"
#include "concurrency/ThreadPool.hpp"
#include "logging/LogRegistry.hpp"
#include "nlohmann/json.hpp"
#include "protocols/websocket/WebSocketSession.hpp"
#include "stats/VaultStatTask.hpp"
#include "types/stats/VaultStat.hpp"
#include "types/User.hpp"
#include "types/stats/CacheStats.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "storage/FSCache.hpp"

#include <future>

using namespace vh::websocket;
using namespace vh::types;
using namespace vh::logging;
using namespace vh::stats;
using namespace vh::services;

void StatsHandler::handleVaultStats(const json& msg, WebSocketSession& session) {
    LogRegistry::ws()->debug("[StatsHandler::handleVaultStats] vault stats requested.");
    try {
        const auto& payload = msg.at("payload");
        const auto vaultId = payload.at("vault_id");

        const auto user = session.getAuthenticatedUser();
        if (!user) throw std::runtime_error("Invalid user");
        if (!user->canManageVault(vaultId)) throw std::runtime_error(
            "User does not have permission to manage this vault.");

        const auto task = std::make_shared<VaultStatTask>(vaultId);
        auto future = task->getFuture().value();
        concurrency::ThreadPoolManager::instance().statsPool()->submit(task);

        const auto result = future.get();
        if (const auto stats = std::get<std::shared_ptr<VaultStat> >(result)) {
            const json data = {{"stats", stats}};
            const json response = {{"command", "stats.vault.response"},
                                   {"status", "ok"},
                                   {"requestId", msg.at("requestId").get<std::string>()},
                                   {"data", data}};

            session.send(response);
            LogRegistry::ws()->debug("[StatsHandler::handleVaultStats] vault.response");
        } else throw std::runtime_error("Failed to get stats from vault.");
    } catch (const std::exception& e) {
        LogRegistry::ws()->debug("[StatsHandler::handleVaultStats] exception caught: %s", e.what());
        const json response = {{"command", "stats.vault.response"}, {"status", "error"}, {"error", e.what()}};
        session.send(response);
    }
}

void StatsHandler::handleCacheStats(const json& msg, WebSocketSession& session) {
    LogRegistry::ws()->debug("[StatsHandler::handleCacheStats] cache stats requested.");
    try {
        if (const auto user = session.getAuthenticatedUser(); !user->isAdmin()) throw std::runtime_error(
            "Must be an admin to view cache stats.");

        const auto stats = ServiceDepsRegistry::instance().fsCache->stats();
        if (!stats) throw std::runtime_error("No cache stats available.");

        const json data = {{"stats", stats}};
        const json response = {{"command", "stats.cache.response"},
                               {"status", "ok"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"data", data}};
        session.send(response);
        LogRegistry::ws()->debug("[StatsHandler::handleCacheStats] stats.cache.response");
    } catch (const std::exception& e) {
        LogRegistry::ws()->debug("[StatsHandler::handleCacheStats] exception caught: %s", e.what());
        const json response = {{"command", "stats.cache.response"}, {"status", "error"}, {"error", e.what()}};
        session.send(response);
    }
}