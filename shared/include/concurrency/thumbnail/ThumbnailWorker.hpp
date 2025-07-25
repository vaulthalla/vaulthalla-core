#pragma once

#include "engine/StorageEngineBase.hpp"
#include "types/File.hpp"
#include "ThumbnailTask.hpp"
#include "concurrency/ThreadPool.hpp"
#include "concurrency/SharedThreadPoolRegistry.hpp"

#include <memory>
#include <string>
#include <iostream>

namespace vh::concurrency {

struct ThumbnailWorker {
    static void enqueue(const std::shared_ptr<engine::StorageEngineBase>& engine,
             const std::vector<uint8_t>& buffer,
             const std::shared_ptr<types::File>& file) {
        try {
            const std::string& mime = file->mime_type ? *file->mime_type : "unknown";
            if (!(mime.starts_with("image/") || mime.starts_with("application/"))) return;
            auto task = std::make_unique<ThumbnailTask>(engine, buffer, file);
            SharedThreadPoolRegistry::instance().thumbPool()->submit(std::move(task));
        } catch (const std::exception& e) {
            std::cerr << "[ThumbnailWorker] Failed to enqueue task: " << e.what() << std::endl;
        }
    }
};

}
