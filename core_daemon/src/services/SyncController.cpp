#include "services/SyncController.hpp"
#include "storage/StorageManager.hpp"
#include "concurrency/ThreadPoolRegistry.hpp"
#include "concurrency/sync/CacheSyncTask.hpp"
#include "concurrency/sync/SafeSyncTask.hpp"
#include "concurrency/sync/MirrorSyncTask.hpp"
#include "concurrency/ThreadPool.hpp"
#include "storage/CloudStorageEngine.hpp"
#include "database/Queries/VaultQueries.hpp"
#include "types/Sync.hpp"

#include <boost/dynamic_bitset.hpp>
#include <iostream>
#include <thread>

using namespace vh::services;
using namespace vh::storage;
using namespace vh::concurrency;

bool SyncTaskCompare::operator()(const std::shared_ptr<SyncTask>& a, const std::shared_ptr<SyncTask>& b) const {
    return a->next_run > b->next_run; // Min-heap based on next_run time
}

SyncController::SyncController(std::shared_ptr<StorageManager> storage_manager)
    : storage_(std::move(storage_manager)),
      pool_(ThreadPoolRegistry::instance().syncPool()) {
    if (!storage_) throw std::runtime_error("Storage manager is not initialized");
}

SyncController::~SyncController() {
    stop();
    if (pool_) pool_->stop();
}

void SyncController::start() {
    if (running_) return;
    running_ = true;

    const auto self = shared_from_this();
    controllerThread_ = std::thread([self]() { self->run(); });

    std::cout << "[SyncController] Started." << std::endl;
}

void SyncController::stop() {
    running_ = false;
    if (controllerThread_.joinable()) controllerThread_.join();
    std::cout << "[SyncController] Stopped." << std::endl;
}

void SyncController::requeue(const std::shared_ptr<SyncTask>& task) {
    std::scoped_lock lock(pqMutex_);
    pq.push(task);
    std::cout << "[SyncController] Requeued sync task for vault ID: " << task->vaultId() << std::endl;
}

void SyncController::run() {
    std::vector<std::shared_ptr<CloudStorageEngine>> engineBuffer;

    const auto refreshEngines = [&]() {
        engineBuffer.clear();

        const auto latestEngines = storage_->getEngines<CloudStorageEngine>();

        boost::dynamic_bitset<> latestBitset(database::VaultQueries::maxVaultId() + 1);
        for (const auto& engine : latestEngines) latestBitset.set(engine->getVault()->id);

        {
            std::unique_lock lock(engineMapMutex_);

            // Remove engines that are no longer present
            std::vector<unsigned int> staleIds;

            for (const auto& [id, engine] : engineMap_)
                if (!latestBitset[engine->getVault()->id]) staleIds.push_back(id);

            for (auto id : staleIds) engineMap_.erase(id);

            // Add new engines that are not already in the map
            for (const auto& engine : latestEngines) {
                if (!engineMap_.contains(engine->getVault()->id)) {
                    engineMap_[engine->getVault()->id] = {engine};
                    engineBuffer.push_back(engine);
                }
            }
        }

        if (engineBuffer.empty()) return;

        // Add sync tasks for each engine
        std::scoped_lock lock(pqMutex_);
        for (const auto& engine : engineBuffer) {
            if (engine->sync->strategy == types::Sync::Strategy::Cache)
                pq.push(std::make_shared<CacheSyncTask>(engine, shared_from_this()));
            else if (engine->sync->strategy == types::Sync::Strategy::Sync)
                pq.push(std::make_shared<SafeSyncTask>(engine, shared_from_this()));
            else if (engine->sync->strategy == types::Sync::Strategy::Mirror)
                pq.push(std::make_shared<MirrorSyncTask>(engine, shared_from_this()));
            else std::cerr << "[SyncController] Unsupported sync strategy for vault ID: " << engine->vaultId() << std::endl;
        }
    };

    refreshEngines();

    std::chrono::system_clock::time_point lastRefresh = std::chrono::system_clock::now();
    unsigned int refreshTries = 0;

    while (running_) {
        if (pool_->interrupted()) {
            std::cout << "[SyncController] Interrupted, stopping." << std::endl;
            return;
        }

        if (std::chrono::system_clock::now() - lastRefresh > std::chrono::minutes(5)) {
            std::cout << "[SyncController] Refreshing cloud storage engines." << std::endl;
            lastRefresh = std::chrono::system_clock::now();
        }

        bool pqIsEmpty = false;
        {
            std::scoped_lock lock(pqMutex_);
            pqIsEmpty = pq.empty();
        }

        if (pqIsEmpty) {
            if (++refreshTries > 3) std::this_thread::sleep_for(std::chrono::seconds(30));
            std::cout << "[SyncController] No sync tasks available." << std::endl;
            refreshEngines();
        } else {
            refreshTries = 0;
            std::shared_ptr<SyncTask> task;

            {
                std::scoped_lock lock(pqMutex_);
                task = pq.top();
            }

            if (task->next_run > std::chrono::system_clock::now()) std::this_thread::sleep_until(task->next_run);
            else {
                std::scoped_lock lock(pqMutex_);
                pq.pop();
                pool_->submit(task);
            }
        }
    }
}
