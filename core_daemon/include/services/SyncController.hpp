#pragma once

#include "../../../shared/include/concurrency/FSTask.hpp"

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
}

namespace vh::services {

struct FSTaskCompare {
    bool operator()(const std::shared_ptr<concurrency::FSTask>& a, const std::shared_ptr<concurrency::FSTask>& b) const;
};

class SyncController : public std::enable_shared_from_this<SyncController> {
public:
    explicit SyncController(const std::weak_ptr<storage::StorageManager>& storage_manager);
    ~SyncController();

    void start();
    void stop();

    void requeue(const std::shared_ptr<concurrency::FSTask>& task);

    void interruptTask(unsigned int vaultId);

    void runNow(unsigned int vaultId);

private:
    friend class concurrency::FSTask;
    friend class concurrency::SyncTask;

    std::priority_queue<std::shared_ptr<concurrency::FSTask>,
                    std::vector<std::shared_ptr<concurrency::FSTask>>,
                    FSTaskCompare> pq;

    std::weak_ptr<storage::StorageManager> storage_;
    std::shared_ptr<concurrency::ThreadPool> pool_;
    std::thread controllerThread_;
    std::atomic<bool> running_{false};

    mutable std::mutex pqMutex_;
    mutable std::shared_mutex taskMapMutex_;

    std::unordered_map<unsigned int, std::shared_ptr<concurrency::FSTask>> taskMap_{};

    void run();

    void refreshEngines();

    void pruneStaleTasks(const std::vector<std::shared_ptr<storage::StorageEngine>>& engines);

    void processTask(const std::shared_ptr<storage::StorageEngine>& engine);

    std::shared_ptr<concurrency::FSTask> createTask(const std::shared_ptr<storage::StorageEngine>& engine);

    template <typename T>
    std::shared_ptr<T> createTask(const std::shared_ptr<storage::StorageEngine>& engine) {
        auto task = std::make_shared<T>(engine, shared_from_this());
        return task;
    }
};

}
