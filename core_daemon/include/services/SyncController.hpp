#pragma once

#include <memory>
#include <queue>
#include <atomic>
#include <thread>

namespace vh::concurrency {
class ThreadPool;
}

namespace vh::storage {
class StorageManager;
}

namespace vh::cloud {
class SyncTask;
}

namespace vh::services {

class SyncController : public std::enable_shared_from_this<SyncController> {
public:
    explicit SyncController(std::shared_ptr<storage::StorageManager> storage_manager);
    ~SyncController();

    void start();
    void stop();

private:
    void run();
    std::priority_queue<std::shared_ptr<cloud::SyncTask>> pq{};

    std::shared_ptr<storage::StorageManager> storage_;
    std::shared_ptr<concurrency::ThreadPool> pool_;
    std::thread controllerThread_;
    std::atomic<bool> running_{false};

    friend class cloud::SyncTask;
};

}
