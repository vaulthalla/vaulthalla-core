#pragma once

#include <memory>
#include <queue>

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
    std::priority_queue<std::shared_ptr<cloud::SyncTask>> pq{};

    explicit SyncController(std::shared_ptr<storage::StorageManager> storage_manager);
    ~SyncController();

    void operator()();

private:
    std::shared_ptr<storage::StorageManager> storage_;
    std::shared_ptr<concurrency::ThreadPool> pool_;
};

}
