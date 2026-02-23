#pragma once

#include "concurrency/Task.hpp"
#include "types/sync/Throughput.hpp"

#include <memory>
#include <chrono>
#include <future>
#include <atomic>
#include <vector>

namespace vh::storage {
class StorageEngine;
}

namespace vh::services {
class SyncController;
}

namespace vh::types {
struct File;

namespace sync { struct Event; }

}

namespace vh::concurrency {

struct Stage {
    const char* name;
    std::function<void()> fn;
};

struct FSTask : Task, std::enable_shared_from_this<FSTask> {
    std::chrono::system_clock::time_point next_run;
    std::shared_ptr<storage::StorageEngine> engine;
    std::vector<std::future<ExpectedFuture>> futures;
    bool runningFlag{false}, runNowFlag{false};
    std::atomic<bool> interruptFlag{false};
    std::shared_ptr<types::sync::Event> event;
    uint8_t trigger{3};

    FSTask() = default;
    ~FSTask() override = default;

    explicit FSTask(const std::shared_ptr<storage::StorageEngine>& engine);

    void operator()() override;

    [[nodiscard]] unsigned int vaultId() const;

    [[nodiscard]] bool isRunning() const;

    void interrupt();

    [[nodiscard]] bool isInterrupted() const;

    void requeue();

    void runNow(uint8_t trigger = 3);  // sync::Event::Trigger::WEBHOOK

    types::sync::ScopedOp& op(const types::sync::Throughput::Metric& metric) const;

    virtual void processFutures();

    void push(const std::shared_ptr<Task>& task);
    void handleInterrupt() const;
    void processOperations() const;
    void handleVaultKeyRotation();
    void removeTrashedFiles();

    void startTask();
    void processSharedOps();
    void handleError(const std::string& message) const;
    void shutdown();
    void newEvent();
    void runStages(std::span<const Stage> stages) const;
};

}
