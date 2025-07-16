#pragma once

#include "concurrency/Task.hpp"
#include "storage/CloudStorageEngine.hpp"
#include "storage/StorageEngine.hpp"
#include "types/File.hpp"
#include "types/Vault.hpp"
#include "types/CacheIndex.hpp"

#include <atomic>
#include <memory>
#include <iostream>

namespace vh::concurrency {

struct DownloadTask : PromisedTask {
    std::shared_ptr<storage::CloudStorageEngine> engine;
    std::shared_ptr<types::File> file;
    std::atomic<uintmax_t>* free;

    DownloadTask(std::shared_ptr<storage::CloudStorageEngine> eng,
                 std::shared_ptr<types::File> f,
                 std::atomic<uintmax_t>* fr)
        : engine(std::move(eng)), file(std::move(f)), free(fr) {}

    void operator()() override {
        try {
            if (shouldPurgeNewFiles()) engine->indexAndDeleteFile(file->path);
            else {
                const auto index = engine->cacheFile(file->path);
                free->fetch_sub(index->size, std::memory_order_relaxed);
            }
            promise.set_value(true);
        } catch (const std::exception& e) {
            std::cerr << "[DownloadTask] Error: " << e.what() << " (path = " << file->path << ")\n";
            promise.set_value(false); // or wrap exception if desired
        }
    }

    bool shouldPurgeNewFiles() const {
        return engine->getVault()->quota != 0 && *free < storage::StorageEngine::MIN_FREE_SPACE * 2; // 20MB threshold
    }
};


}
