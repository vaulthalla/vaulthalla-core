#pragma once

#include "config/ConfigRegistry.hpp"
#include "util/imageUtil.hpp"
#include "storage/StorageEngine.hpp"

#include <string>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <filesystem>
#include <atomic>
#include <functional>
#include <iostream>

namespace vh::services {

class ThumbnailWorker {
public:
    struct Job {
        std::shared_ptr<const storage::StorageEngine> engine;
        std::string buffer;
        std::filesystem::path rel_path;
        std::string mime_type;
    };

    using CachePathResolver = std::function<std::filesystem::path(
        const std::filesystem::path& rel_path,
        const std::filesystem::path& subpath)>;

    explicit ThumbnailWorker() : stopFlag(false) {}

    void start() {
        workerThread = std::thread([this] { run(); });
    }

    void enqueue(const std::shared_ptr<storage::StorageEngine>& engine, const std::string& buffer,
                 const std::filesystem::path& rel_path, const std::string& mime_type) { {
            std::lock_guard lock(queueMutex);
            jobs.push(Job{engine, buffer, rel_path, mime_type});
        }
        queueCond.notify_one();
    }

    void stop() { {
            std::lock_guard lock(queueMutex);
            stopFlag = true;
        }
        queueCond.notify_all();
        if (workerThread.joinable()) workerThread.join();
    }

private:
    std::queue<Job> jobs;
    std::mutex queueMutex;
    std::condition_variable queueCond;
    std::thread workerThread;
    std::atomic<bool> stopFlag;

    void run() {
        while (!stopFlag) {
            Job job; {
                std::unique_lock lock(queueMutex);
                queueCond.wait(lock, [this] { return stopFlag || !jobs.empty(); });
                if (stopFlag && jobs.empty()) break;

                job = std::move(jobs.front());
                jobs.pop();
            }

            processJob(job);
        }
    }

    void processJob(const Job& job) const {
        using namespace std;
        namespace fs = std::filesystem;

        try {
            const auto& sizes = config::ConfigRegistry::get().caching.thumbnails.sizes;
            for (const auto& size : sizes) {
                auto cachePath = job.engine->getAbsoluteCachePath(job.rel_path, fs::path("thumbnails") / to_string(size));
                if (cachePath.extension() != ".jpg" && cachePath.extension() != ".jpeg") cachePath += ".jpg";
                if (!fs::exists(cachePath.parent_path())) fs::create_directories(cachePath.parent_path());
                util::generateAndStoreThumbnail(job.buffer, cachePath, job.mime_type, size);
            }
        } catch (const std::exception& e) {
            std::cerr << "[ThumbnailWorker] Failed to generate thumbnail(s): " << e.what() << std::endl;
        }
    }
};

}