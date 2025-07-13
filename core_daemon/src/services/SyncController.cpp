#include "services/SyncController.hpp"
#include "storage/StorageManager.hpp"
#include "concurrency/ThreadPoolRegistry.hpp"
#include "cloud/SyncTask.hpp"
#include "concurrency/TaskWrapper.hpp"
#include "concurrency/ThreadPool.hpp"
#include "storage/CloudStorageEngine.hpp"

#include <iostream>

using namespace vh::services;

SyncController::SyncController(std::shared_ptr<storage::StorageManager> storage_manager)
    : storage_(std::move(storage_manager)),
      pool_(concurrency::ThreadPoolRegistry::instance().syncPool()) {
    if (!storage_) throw std::runtime_error("Storage manager is not initialized");
}

SyncController::~SyncController() { if (pool_) pool_->stop(); }

void SyncController::operator()() {
    const auto cloudEngines = storage_->getEngines<storage::CloudStorageEngine>();

    if (cloudEngines.empty()) {
        std::cerr << "[SyncController] No cloud storage engines found, exiting sync controller." << std::endl;
        return;
    }

    for (const auto& cloudEngine : cloudEngines)
        pq.push(std::make_shared<cloud::SyncTask>(cloudEngine, shared_from_this()));

    while (true) {
        if (pool_->interrupted()) {
            std::cout << "[SyncController] Sync controller interrupted, stopping." << std::endl;
            return;
        }

        if (pq.empty()) std::this_thread::sleep_for(std::chrono::seconds(5));
        else {
            const auto task = pq.top();
            if (task->next_run > std::chrono::system_clock::now()) std::this_thread::sleep_until(task->next_run);
            else {
                pq.pop();
                pool_->submit(concurrency::TaskWrapper(task));
            }
        }
    }
}
