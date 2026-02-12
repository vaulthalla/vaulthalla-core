#pragma once

#include "storage/StorageEngine.hpp"
#include "types/fs/File.hpp"
#include "concurrency/thumbnail/ThumbnailTask.hpp"
#include "concurrency/ThreadPool.hpp"
#include "concurrency/ThreadPoolManager.hpp"
#include "logging/LogRegistry.hpp"

#include <memory>
#include <string>

using namespace vh::concurrency;
using namespace vh::logging;

namespace vh::services {

struct ThumbnailWorker {
    static void enqueue(const std::shared_ptr<storage::StorageEngine>& engine,
             const std::vector<uint8_t>& buffer,
             const std::shared_ptr<types::File>& file) {
        try {
            const std::string& mime = file->mime_type ? *file->mime_type : "unknown";
            if (!(mime.starts_with("image/") || mime.starts_with("application/"))) return;
            auto task = std::make_unique<ThumbnailTask>(engine, buffer, file);
            ThreadPoolManager::instance().thumbPool()->submit(std::move(task));
        } catch (const std::exception& e) {
            LogRegistry::thumb()->error("[ThumbnailWorker] Failed to enqueue thumbnail task: {}", e.what());
        }
    }
};

}
