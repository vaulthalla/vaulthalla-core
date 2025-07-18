#pragma once

#include "concurrency/Task.hpp"
#include "concurrency/sync/DeleteTask.hpp"

#include <memory>
#include <unordered_map>
#include <string>
#include <chrono>
#include <filesystem>
#include <future>
#include <atomic>
#include <vector>

namespace vh::services {
class SyncController;
}

namespace vh::storage {
class CloudStorageEngine;
}

namespace vh::types {
struct File;
struct Directory;
}

namespace vh::concurrency {

class SyncTask : public Task, public std::enable_shared_from_this<SyncTask> {
public:
    std::chrono::system_clock::time_point next_run;

    ~SyncTask() override = default;

    SyncTask(const std::shared_ptr<storage::CloudStorageEngine>& engine,
             const std::shared_ptr<services::SyncController>& controller);

    void operator()() override;

    virtual void sync() = 0;

    unsigned int vaultId() const;

    bool operator<(const SyncTask& other) const;

    [[nodiscard]] bool isRunning() const { return isRunning_; }

    void interrupt();

    [[nodiscard]] bool isInterrupted() const;

    std::shared_ptr<storage::CloudStorageEngine> engine() const { return engine_; }

protected:
    std::shared_ptr<storage::CloudStorageEngine> engine_;
    std::shared_ptr<services::SyncController> controller_;
    std::vector<std::future<ExpectedFuture>> futures_;
    std::vector<std::shared_ptr<types::File>> localFiles_, s3Files_;
    std::unordered_map<std::u8string, std::shared_ptr<types::File>> localMap_, s3Map_;
    std::unordered_map<std::u8string, std::optional<std::string>> remoteHashMap_;
    bool isRunning_ = false;
    std::atomic<bool> interruptFlag_{false};

    void push(const std::shared_ptr<Task>& task);
    void upload(const std::shared_ptr<types::File>& file);
    void download(const std::shared_ptr<types::File>& file, bool freeAfterDownload = false);
    void remove(const std::shared_ptr<types::File>& file, const DeleteTask::Type& type = DeleteTask::Type::PURGE);

    void handleInterrupt() const;

    virtual void removeTrashedFiles();

    virtual void ensureFreeSpace(uintmax_t size) const;

    void processFutures();

    static uintmax_t computeReqFreeSpaceForDownload(const std::vector<std::shared_ptr<types::File>>& files);

    static std::vector<std::shared_ptr<types::File>> uMap2Vector(
        std::unordered_map<std::u8string, std::shared_ptr<types::File>>& map);

    static std::unordered_map<std::u8string, std::shared_ptr<types::File>> intersect(
        const std::unordered_map<std::u8string, std::shared_ptr<types::File>>& a,
        const std::unordered_map<std::u8string, std::shared_ptr<types::File>>& b);

    static std::unordered_map<std::u8string, std::shared_ptr<types::File> > symmetric_diff(
        const std::unordered_map<std::u8string, std::shared_ptr<types::File>>& a,
        const std::unordered_map<std::u8string, std::shared_ptr<types::File>>& b);
};

}
