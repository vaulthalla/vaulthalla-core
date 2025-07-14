#pragma once

#include "concurrency/Task.hpp"

#include <memory>
#include <unordered_map>
#include <string>
#include <chrono>
#include <filesystem>

namespace vh::services {
class SyncController;
}

namespace vh::storage {
struct CloudStorageEngine;
}

namespace vh::types {
struct File;
struct Directory;
}

namespace vh::cloud {

class SyncTask : public concurrency::Task, public std::enable_shared_from_this<SyncTask> {
public:
    std::chrono::system_clock::time_point next_run;

    explicit SyncTask(const std::shared_ptr<storage::CloudStorageEngine>& engine,
                      const std::shared_ptr<services::SyncController>& syncController);

    void operator()() override;

    bool operator<(const SyncTask& other) const;

private:
    inline static const std::string CONTENT_HASH_ID = "x-amz-meta-content-hash";

    std::shared_ptr<storage::CloudStorageEngine> engine_;
    std::shared_ptr<services::SyncController> controller_;

    void sync(std::unordered_map<std::u8string, std::shared_ptr<types::File>>& s3Map) const;

    void downloadDiff(std::unordered_map<std::u8string, std::shared_ptr<types::File>>& s3Map) const;

    void cacheDiff(std::unordered_map<std::u8string, std::shared_ptr<types::File>>& s3Map) const;

    static std::vector<std::shared_ptr<types::File>> uMap2Vector(std::unordered_map<std::u8string, std::shared_ptr<types::File>>& map);

    static std::string getMimeType(const std::filesystem::path& path);

    std::vector<std::shared_ptr<types::Directory>> extractDirectories(const std::vector<std::shared_ptr<types::File>>& files) const;
};

}
