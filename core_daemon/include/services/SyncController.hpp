#pragma once

#include "concurrency/sync/SyncTask.hpp"

#include <memory>
#include <queue>
#include <atomic>
#include <thread>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>

namespace vh::concurrency {
class ThreadPool;
}

namespace vh::storage {
class StorageManager;
class CloudStorageEngine;
}

namespace vh::services {

struct SyncTaskCompare {
    bool operator()(const std::shared_ptr<concurrency::SyncTask>& a, const std::shared_ptr<concurrency::SyncTask>& b) const;
};

class SyncController : public std::enable_shared_from_this<SyncController> {
public:
    explicit SyncController(std::shared_ptr<storage::StorageManager> storage_manager);
    ~SyncController();

    void start();
    void stop();

    void requeue(const std::shared_ptr<concurrency::SyncTask>& task);

private:
    void run();
    std::priority_queue<std::shared_ptr<concurrency::SyncTask>,
                    std::vector<std::shared_ptr<concurrency::SyncTask>>,
                    SyncTaskCompare> pq;

    std::shared_ptr<storage::StorageManager> storage_;
    std::shared_ptr<concurrency::ThreadPool> pool_;
    std::thread controllerThread_;
    std::atomic<bool> running_{false};

    mutable std::mutex pqMutex_;
    mutable std::shared_mutex engineMapMutex_;

    std::unordered_map<unsigned int, std::shared_ptr<storage::CloudStorageEngine>> engineMap_{};

    friend class concurrency::SyncTask;
};

}
