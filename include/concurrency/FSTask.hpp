#pragma once

#include "concurrency/Task.hpp"
#include "types/sync/Throughput.hpp"

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

namespace vh::types {
struct File;

namespace sync { struct Event; }

}

namespace vh::concurrency {

struct Stage {
    const char* name;
    std::function<void()> fn;
};

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

    void runNow(uint8_t trigger = 3);  // sync::Event::Trigger::WEBHOOK

protected:
    std::shared_ptr<storage::StorageEngine> engine_;
    std::vector<std::future<ExpectedFuture>> futures_;
    bool isRunning_{false}, runNow_{false};
    std::atomic<bool> interruptFlag_{false};
    std::shared_ptr<types::sync::Event> event_;
    uint8_t trigger_{3};

    void startTask();
    void processSharedOps();
    void handleError(const std::string& message) const;
    void shutdown();
    void newEvent();
    void runStages(std::span<const Stage> stages) const;

    types::sync::ScopedOp& op(const types::sync::Throughput::Metric& metric) const;

    virtual void removeTrashedFiles() = 0;
    virtual void processFutures();
    virtual void pushKeyRotationTask(const std::vector<std::shared_ptr<types::File>>& files,
                                     unsigned int begin, unsigned int end) = 0;

    void push(const std::shared_ptr<Task>& task);
    void handleInterrupt() const;
    void processOperations() const;
    void handleVaultKeyRotation();
};

}
