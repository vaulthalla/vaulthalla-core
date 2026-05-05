#include "stats/SnapshotService.hpp"

#include "config/Registry.hpp"
#include "db/query/stats/DbStats.hpp"
#include "db/query/stats/Snapshot.hpp"
#include "db/query/sync/Stats.hpp"
#include "db/query/vault/Activity.hpp"
#include "fs/cache/Registry.hpp"
#include "log/Registry.hpp"
#include "nlohmann/json.hpp"
#include "runtime/Deps.hpp"
#include "stats/model/CacheStats.hpp"
#include "stats/model/DbStats.hpp"
#include "stats/model/FuseStats.hpp"
#include "stats/model/ThreadPoolStats.hpp"
#include "stats/model/VaultActivity.hpp"
#include "stats/model/VaultSyncHealth.hpp"
#include "storage/Engine.hpp"
#include "storage/Manager.hpp"
#include "vault/model/Capacity.hpp"
#include "vault/model/Vault.hpp"

#include <algorithm>

namespace vh::stats {

namespace {

std::uint64_t statsSnapshotServiceUnixTimestamp() {
    return static_cast<std::uint64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
}

template <typename Func>
void snapshotServiceTry(const char* label, Func&& func) {
    try {
        func();
    } catch (const std::exception& e) {
        log::Registry::runtime()->warn("[StatsSnapshotService] {} snapshot failed: {}", label, e.what());
    }
}

std::chrono::seconds snapshotServiceSeconds(const std::uint32_t seconds) {
    return std::chrono::seconds(std::max<std::uint32_t>(1, seconds));
}

}

SnapshotService::SnapshotService()
    : AsyncService("StatsSnapshotService") {}

void SnapshotService::runLoop() {
    using Clock = std::chrono::steady_clock;

    lastRuntimeCollection_ = Clock::time_point::min();
    lastVaultCollection_ = Clock::time_point::min();
    lastPurge_ = Clock::time_point::min();

    while (!shouldStop()) {
        const auto& config = config::Registry::get().stats_snapshots;

        if (!config.enabled) {
            lazySleep(std::chrono::seconds(60));
            continue;
        }

        const auto now = Clock::now();
        const auto runtimeInterval = snapshotServiceSeconds(config.runtime_interval_seconds);
        const auto vaultInterval = snapshotServiceSeconds(config.vault_interval_seconds);

        if (lastRuntimeCollection_ == Clock::time_point::min() || now - lastRuntimeCollection_ >= runtimeInterval) {
            collectRuntimeSnapshots();
            lastRuntimeCollection_ = now;
        }

        if (lastVaultCollection_ == Clock::time_point::min() || now - lastVaultCollection_ >= vaultInterval) {
            collectVaultSnapshots();
            lastVaultCollection_ = now;
        }

        if (lastPurge_ == Clock::time_point::min() || now - lastPurge_ >= std::chrono::hours(24)) {
            purgeOldSnapshots();
            lastPurge_ = now;
        }

        const auto sleepFor = std::min<std::chrono::seconds>(runtimeInterval, std::chrono::seconds(60));
        lazySleep(sleepFor);
    }
}

void SnapshotService::collectRuntimeSnapshots() {
    auto& deps = runtime::Deps::get();

    snapshotServiceTry("system.threadpools", [] {
        db::query::stats::Snapshot::insertSystem(
            "system.threadpools",
            nlohmann::json(model::ThreadPoolManagerSnapshot::snapshot())
        );
    });

    snapshotServiceTry("system.fuse", [&deps] {
        if (!deps.fuseStats) return;
        db::query::stats::Snapshot::insertSystem("system.fuse", nlohmann::json(deps.fuseStats->snapshot()));
    });

    snapshotServiceTry("system.cache", [&deps] {
        nlohmann::json payload = {
            {"checked_at", statsSnapshotServiceUnixTimestamp()},
        };

        if (deps.fsCache) {
            const auto fsStats = deps.fsCache->stats();
            payload["fs"] = fsStats ? nlohmann::json(*fsStats) : nlohmann::json(nullptr);
        }

        if (deps.httpCacheStats) {
            payload["http"] = nlohmann::json(*deps.httpCacheStats);
        }

        if (payload.contains("fs") || payload.contains("http"))
            db::query::stats::Snapshot::insertSystem("system.cache", payload);
    });

    snapshotServiceTry("system.db", [] {
        const auto stats = db::query::stats::DbStats::snapshot();
        if (stats) db::query::stats::Snapshot::insertSystem("system.db", nlohmann::json(*stats));
    });
}

void SnapshotService::collectVaultSnapshots() {
    auto& deps = runtime::Deps::get();
    if (!deps.storageManager) return;

    const auto engines = deps.storageManager->getEngines();
    for (const auto& engine : engines) {
        if (!engine || !engine->vault || engine->vault->id == 0) continue;
        const auto vaultId = engine->vault->id;

        snapshotServiceTry("vault.capacity", [vaultId] {
            const auto capacity = std::make_shared<vault::model::Capacity>(vaultId);
            db::query::stats::Snapshot::insertVault(vaultId, "vault.capacity", nlohmann::json(capacity));
        });

        snapshotServiceTry("vault.sync", [vaultId] {
            const auto stats = db::query::sync::Stats::getVaultSyncHealth(vaultId);
            if (stats) db::query::stats::Snapshot::insertVault(vaultId, "vault.sync", nlohmann::json(*stats));
        });

        snapshotServiceTry("vault.activity", [vaultId] {
            const auto stats = db::query::vault::Activity::getVaultActivity(vaultId);
            if (stats) db::query::stats::Snapshot::insertVault(vaultId, "vault.activity", nlohmann::json(*stats));
        });
    }
}

void SnapshotService::purgeOldSnapshots() {
    const auto retentionDays = config::Registry::get().stats_snapshots.retention_days;
    snapshotServiceTry("stats_snapshot retention", [retentionDays] {
        db::query::stats::Snapshot::purgeOlderThan(retentionDays);
    });
}

}
