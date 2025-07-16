#pragma once

#include "storage/StorageEngine.hpp"
#include "types/File.hpp"
#include "concurrency/thumbnail/ThumbnailTask.hpp"
#include "concurrency/ThreadPoolRegistry.hpp"
#include "concurrency/ThreadPool.hpp"

#include <memory>
#include <string>
#include <iostream>

namespace vh::concurrency {

class ThumbnailWorker {
public:
    void enqueue(const std::shared_ptr<storage::StorageEngine>& engine,
                 const std::string& buffer,
                 const std::shared_ptr<types::File>& file) const {
        try {
            auto task = std::make_unique<ThumbnailTask>(engine, buffer, file);
            ThreadPoolRegistry::instance().thumbPool()->submit(std::move(task));
        } catch (const std::exception& e) {
            std::cerr << "[ThumbnailWorker] Failed to enqueue task: " << e.what() << std::endl;
        }
    }
};

}
