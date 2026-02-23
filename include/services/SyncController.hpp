#pragma once

#include "concurrency/AsyncService.hpp"

#include <memory>
#include <queue>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>

namespace vh::storage {
struct StorageEngine;
}

namespace vh::sync {
struct Cloud;
struct Local;
}

namespace vh::services {

struct FSTaskCompare {
    bool operator()(const std::shared_ptr<sync::Local>& a, const std::shared_ptr<sync::Local>& b) const;
};

class SyncController final : public concurrency::AsyncService, std::enable_shared_from_this<SyncController> {
public:
    SyncController();
    ~SyncController() override = default;

    void requeue(const std::shared_ptr<sync::Local>& task);

    void interruptTask(unsigned int vaultId);

    void runNow(unsigned int vaultId, uint8_t trigger = 3); // sync::Event::Trigger::WEBHOOK

protected:
    void runLoop() override;

private:
    friend class sync::Local;
    friend class sync::Cloud;

    std::priority_queue<std::shared_ptr<sync::Local>,
                    std::vector<std::shared_ptr<sync::Local>>,
                    FSTaskCompare> pq;

    mutable std::mutex pqMutex_;
    mutable std::shared_mutex taskMapMutex_;

    std::unordered_map<unsigned int, std::shared_ptr<sync::Local>> taskMap_{};

    void refreshEngines();

    void pruneStaleTasks(const std::vector<std::shared_ptr<storage::StorageEngine>>& engines);

    void processTask(const std::shared_ptr<storage::StorageEngine>& engine);

    std::shared_ptr<sync::Local> createTask(const std::shared_ptr<storage::StorageEngine>& engine);

    template <typename T>
    std::shared_ptr<T> createTask(const std::shared_ptr<storage::StorageEngine>& engine) {
        auto task = std::make_shared<T>(engine);
        return task;
    }
};

}
