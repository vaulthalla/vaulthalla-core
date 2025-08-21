#pragma once

#include "services/AsyncService.hpp"

#include <memory>
#include <queue>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>

namespace vh::storage {
class StorageEngine;
}

namespace vh::concurrency {
class SyncTask;
class FSTask;
}

namespace vh::services {

struct FSTaskCompare {
    bool operator()(const std::shared_ptr<concurrency::FSTask>& a, const std::shared_ptr<concurrency::FSTask>& b) const;
};

class SyncController final : public AsyncService, std::enable_shared_from_this<SyncController> {
public:
    SyncController();
    ~SyncController() override = default;

    void requeue(const std::shared_ptr<concurrency::FSTask>& task);

    void interruptTask(unsigned int vaultId);

    void runNow(unsigned int vaultId);

protected:
    void runLoop() override;

private:
    friend class concurrency::FSTask;
    friend class concurrency::SyncTask;

    std::priority_queue<std::shared_ptr<concurrency::FSTask>,
                    std::vector<std::shared_ptr<concurrency::FSTask>>,
                    FSTaskCompare> pq;

    mutable std::mutex pqMutex_;
    mutable std::shared_mutex taskMapMutex_;

    std::unordered_map<unsigned int, std::shared_ptr<concurrency::FSTask>> taskMap_{};

    void refreshEngines();

    void pruneStaleTasks(const std::vector<std::shared_ptr<storage::StorageEngine>>& engines);

    void processTask(const std::shared_ptr<storage::StorageEngine>& engine);

    std::shared_ptr<concurrency::FSTask> createTask(const std::shared_ptr<storage::StorageEngine>& engine);

    template <typename T>
    std::shared_ptr<T> createTask(const std::shared_ptr<storage::StorageEngine>& engine) {
        auto task = std::make_shared<T>(engine);
        return task;
    }
};

}
