#pragma once

#include "concurrency/Task.hpp"

#include <memory>
#include <unordered_map>
#include <string>
#include <chrono>
#include <filesystem>

#include "storage/CloudStorageEngine.hpp"

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

    virtual ~SyncTask() = default;
    SyncTask(const std::shared_ptr<storage::CloudStorageEngine>& engine,
             const std::shared_ptr<services::SyncController>& controller);

    virtual void operator()();
    virtual void sync(std::unordered_map<std::u8string, std::shared_ptr<types::File>>& s3Map) const = 0;
    virtual void handleDiff(std::unordered_map<std::u8string, std::shared_ptr<types::File>>& s3Map) const = 0;

    unsigned int vaultId() const;
    bool operator<(const SyncTask& other) const;

protected:
    std::shared_ptr<storage::CloudStorageEngine> engine_;
    std::shared_ptr<services::SyncController> controller_;
    mutable uintmax_t free_{0};

    static std::vector<std::shared_ptr<types::File>> uMap2Vector(
        std::unordered_map<std::u8string, std::shared_ptr<types::File>>& map) ;
};


}
