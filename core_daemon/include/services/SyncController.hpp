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
class SyncTask;
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
    explicit SyncController(const std::weak_ptr<storage::StorageManager>& storage_manager);
    ~SyncController();

    void start();
    void stop();

    void requeue(const std::shared_ptr<concurrency::SyncTask>& task);

    void runNow(unsigned int vaultId);

private:
    friend class concurrency::SyncTask;

    std::priority_queue<std::shared_ptr<concurrency::SyncTask>,
                    std::vector<std::shared_ptr<concurrency::SyncTask>>,
                    SyncTaskCompare> pq;

    std::weak_ptr<storage::StorageManager> storage_;
    std::shared_ptr<concurrency::ThreadPool> pool_;
    std::thread controllerThread_;
    std::atomic<bool> running_{false};

    mutable std::mutex pqMutex_;
    mutable std::shared_mutex taskMapMutex_;

    std::unordered_map<unsigned int, std::shared_ptr<concurrency::SyncTask>> taskMap_{};

    void run();

    void refreshEngines();

    void pruneStaleTasks(const std::vector<std::shared_ptr<storage::CloudStorageEngine>>& engines);

    void processTask(const std::shared_ptr<storage::CloudStorageEngine>& engine);

    std::shared_ptr<concurrency::SyncTask> createTask(const std::shared_ptr<storage::CloudStorageEngine>& engine);

    template <typename T>
    std::shared_ptr<T> createTask(const std::shared_ptr<storage::CloudStorageEngine>& engine) {
        auto task = std::make_shared<T>(engine, shared_from_this());
        return task;
    }
};

}
