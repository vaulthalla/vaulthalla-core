#pragma once

#include "../FSTask.hpp"
#include "DeleteTask.hpp"

#include <memory>
#include <unordered_map>
#include <string>
#include <filesystem>
#include <future>
#include <vector>

namespace vh::types {
struct File;
}

namespace vh::storage {
class CloudStorageEngine;
}

namespace vh::concurrency {

class SyncTask : public FSTask {
public:
    ~SyncTask() override = default;

    SyncTask(const std::shared_ptr<storage::StorageEngine>& engine,
             const std::shared_ptr<services::SyncController>& controller) : FSTask(engine, controller) {}

    void operator()() override;

    virtual void sync() = 0;

protected:
    std::vector<std::shared_ptr<types::File>> localFiles_, s3Files_;
    std::unordered_map<std::u8string, std::shared_ptr<types::File>> localMap_, s3Map_;
    std::unordered_map<std::u8string, std::optional<std::string>> remoteHashMap_;

    void upload(const std::shared_ptr<types::File>& file);
    void download(const std::shared_ptr<types::File>& file, bool freeAfterDownload = false);
    void remove(const std::shared_ptr<types::File>& file, const DeleteTask::Type& type = DeleteTask::Type::PURGE);

    std::shared_ptr<storage::CloudStorageEngine> cloudEngine() const;

    void removeTrashedFiles() override;

    virtual void ensureFreeSpace(uintmax_t size) const;

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
