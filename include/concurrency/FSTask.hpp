#pragma once

#include "concurrency/Task.hpp"

#include <memory>
#include <chrono>
#include <future>
#include <atomic>
#include <vector>
#include <utility>

namespace vh::storage {
class StorageEngine;
}

namespace vh::services {
class SyncController;
}

namespace vh::concurrency {

class FSTask : public Task, public std::enable_shared_from_this<FSTask> {
public:
    std::chrono::system_clock::time_point next_run;

    FSTask() = default;
    ~FSTask() override = default;

    explicit FSTask(const std::shared_ptr<storage::StorageEngine>& engine);

    void operator()() override = 0;

    [[nodiscard]] unsigned int vaultId() const;

    std::shared_ptr<storage::StorageEngine> engine() const;

    [[nodiscard]] bool isRunning() const;

    void interrupt();

    [[nodiscard]] bool isInterrupted() const;

    void requeue();

protected:
    std::shared_ptr<storage::StorageEngine> engine_;
    std::vector<std::future<ExpectedFuture>> futures_;
    bool isRunning_ = false;
    std::atomic<bool> interruptFlag_{false};

    void push(const std::shared_ptr<Task>& task);

    void handleInterrupt() const;

    virtual void removeTrashedFiles() = 0;

    void processOperations() const;

    virtual void handleVaultKeyRotation() = 0;
    virtual void processFutures();
};

}
