#include "services/SyncController.hpp"
#include "storage/StorageManager.hpp"
#include "../../include/services/SharedThreadPoolRegistry.hpp"
#include "../../include/tasks/FSTask.hpp"
#include "concurrency/fs/LocalFSTask.hpp"
#include "concurrency/sync/CacheSyncTask.hpp"
#include "concurrency/sync/SafeSyncTask.hpp"
#include "concurrency/sync/MirrorSyncTask.hpp"
#include "concurrency/ThreadPool.hpp"
#include "storage/CloudStorageEngine.hpp"
#include "database/Queries/VaultQueries.hpp"
#include "types/Sync.hpp"
#include "types/FSync.hpp"
#include "types/RSync.hpp"
#include "types/Vault.hpp"

#include <boost/dynamic_bitset.hpp>
#include <iostream>
#include <thread>

using namespace vh::services;
using namespace vh::storage;
using namespace vh::concurrency;

bool FSTaskCompare::operator()(const std::shared_ptr<FSTask>& a, const std::shared_ptr<FSTask>& b) const {
    return a->next_run > b->next_run; // Min-heap based on next_run time
}

SyncController::SyncController(const std::weak_ptr<StorageManager>& storage_manager)
    : storage_(storage_manager),
      pool_(SharedThreadPoolRegistry::instance().syncPool()) {}

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

void SyncController::requeue(const std::shared_ptr<FSTask>& task) {
    std::scoped_lock lock(pqMutex_);
    pq.push(task);
    std::cout << "[SyncController] Requeued sync task for vault ID: " << task->vaultId() << std::endl;
}

void SyncController::interruptTask(const unsigned int vaultId) {
    std::scoped_lock lock(taskMapMutex_, pqMutex_);

    if (!taskMap_.contains(vaultId)) {
        std::cerr << "[SyncController] No sync task found for vault ID: " << vaultId << std::endl;
        return;
    }

    if (const auto& task = taskMap_[vaultId]) {
        task->interrupt();
        std::cout << "[SyncController] Interrupted sync task for vault ID: " << vaultId << std::endl;
    } else std::cerr << "[SyncController] Task for vault ID " << vaultId << " is not running." << std::endl;
}

void SyncController::run() {
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
            refreshEngines();
            lastRefresh = std::chrono::system_clock::now();
        }

        bool pqIsEmpty = false;
        {
            std::scoped_lock lock(pqMutex_);
            pqIsEmpty = pq.empty();
        }

        if (pqIsEmpty) {
            if (++refreshTries > 3) std::this_thread::sleep_for(std::chrono::seconds(3));
            refreshEngines();
            continue;
        }

        refreshTries = 0;
        std::shared_ptr<FSTask> task;

        {
            std::scoped_lock lock(pqMutex_);
            task = pq.top();
            pq.pop();
        }

        if (!task || task->isInterrupted()) continue;

        if (task->next_run <= std::chrono::system_clock::now()) pool_->submit(task);
        else {
            std::scoped_lock lock(pqMutex_);
            pq.push(task);
        }
    }
}

void SyncController::runNow(const unsigned int vaultId) {
    std::cout << "[SyncController] Running sync task immediately for vault ID: " << vaultId << std::endl;

    std::shared_ptr<FSTask> task;

    {
        std::scoped_lock lock(taskMapMutex_);
        if (!taskMap_.contains(vaultId)) {
            std::cerr << "[SyncController] No sync task found for vault ID: " << vaultId << std::endl;
            return;
        }
        task = taskMap_[vaultId];
    }

    task->interrupt();

    while (task->isRunning()) std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const auto engine = task->engine();
    task = createTask(engine);
    task->next_run = std::chrono::system_clock::now();

    {
        std::scoped_lock lock(taskMapMutex_, pqMutex_);
        taskMap_[vaultId] = task;
        pq.push(task);
    }
}

void SyncController::refreshEngines() {
    if (const auto s = storage_.lock()) {
        const auto latestEngines = s->getEngines();
        pruneStaleTasks(latestEngines);
        for (const auto& engine : latestEngines) processTask(engine);
    }
}


void SyncController::pruneStaleTasks(const std::vector<std::shared_ptr<StorageEngine> >& engines) {
    boost::dynamic_bitset<> latestBitset(database::VaultQueries::maxVaultId() + 1);
    for (const auto& engine : engines) latestBitset.set(engine->vault->id);

    {
        std::unique_lock lock(taskMapMutex_);

        // Remove engines that are no longer present
        std::vector<unsigned int> staleIds;

        for (const auto& [id, task] : taskMap_)
            if (!latestBitset[task->vaultId()]) staleIds.push_back(id);

        for (auto id : staleIds) taskMap_.erase(id);
    }
}


void SyncController::processTask(const std::shared_ptr<StorageEngine>& engine) {
    std::scoped_lock lock(taskMapMutex_, pqMutex_);

    if (!taskMap_.contains(engine->vault->id)) {
        const auto task = createTask(engine);
        taskMap_[engine->vault->id] = task;
        pq.push(task);
    }
}

std::shared_ptr<FSTask> SyncController::createTask(const std::shared_ptr<StorageEngine>& engine) {
    std::shared_ptr<FSTask> task;
    if (engine->type() == engine::StorageType::Local) task = createTask<LocalFSTask>(engine);
    else if (engine->type() == engine::StorageType::Cloud) {
        const auto sync = std::static_pointer_cast<types::RSync>(engine->sync);
        if (sync->strategy == types::RSync::Strategy::Cache) task = createTask<CacheSyncTask>(engine);
        else if (sync->strategy == types::RSync::Strategy::Sync) task = createTask<SafeSyncTask>(engine);
        else if (sync->strategy == types::RSync::Strategy::Mirror) task = createTask<MirrorSyncTask>(engine);
    } else std::cerr << "[SyncController] Unsupported sync strategy for vault ID: " << engine->vault->id << std::endl;
    return task;
}


