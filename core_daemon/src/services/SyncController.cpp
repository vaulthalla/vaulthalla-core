#include "services/SyncController.hpp"
#include "storage/StorageManager.hpp"
#include "concurrency/ThreadPoolRegistry.hpp"
#include "cloud/SyncTask.hpp"
#include "concurrency/ThreadPool.hpp"
#include "storage/CloudStorageEngine.hpp"

#include <iostream>
#include <thread>

using namespace vh::services;

SyncController::SyncController(std::shared_ptr<storage::StorageManager> storage_manager)
    : storage_(std::move(storage_manager)),
      pool_(concurrency::ThreadPoolRegistry::instance().syncPool()) {
    if (!storage_) throw std::runtime_error("Storage manager is not initialized");
}

SyncController::~SyncController() {
    stop();
    if (pool_) pool_->stop();
}

void SyncController::start() {
    if (running_) return;
    running_ = true;
    controllerThread_ = std::thread(&SyncController::run, this);
    std::cout << "[SyncController] Started." << std::endl;
}

void SyncController::stop() {
    running_ = false;
    if (controllerThread_.joinable()) controllerThread_.join();
    std::cout << "[SyncController] Stopped." << std::endl;
}

void SyncController::run() {
    const auto cloudEngines = storage_->getEngines<storage::CloudStorageEngine>();

    if (cloudEngines.empty()) {
        std::cerr << "[SyncController] No cloud storage engines found, exiting." << std::endl;
        return;
    }

    auto self = shared_from_this(); // safe to call now — we're inside start()
    for (const auto& cloudEngine : cloudEngines) {
        pq.push(std::make_shared<cloud::SyncTask>(cloudEngine, self));
    }

    while (running_) {
        if (pool_->interrupted()) {
            std::cout << "[SyncController] Interrupted, stopping." << std::endl;
            return;
        }

        if (pq.empty()) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
        } else {
            const auto task = pq.top();
            if (task->next_run > std::chrono::system_clock::now()) {
                std::this_thread::sleep_until(task->next_run);
            } else {
                pq.pop();
                pool_->submit(task);
            }
        }
    }
}
