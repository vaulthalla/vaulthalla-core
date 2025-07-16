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
    bool freeAfterDownload;

    DownloadTask(std::shared_ptr<storage::CloudStorageEngine> eng,
                 std::shared_ptr<types::File> f,
                 bool freeAfter = false)
        : engine(std::move(eng)), file(std::move(f)) {}

    void operator()() override {
        try {
            if (freeAfterDownload) engine->indexAndDeleteFile(file->path);
            else engine->cacheFile(file->path);
            promise.set_value(true);
        } catch (const std::exception& e) {
            std::cerr << "[DownloadTask] Error: " << e.what() << " (path = " << file->path << ")\n";
            promise.set_value(false); // or wrap exception if desired
        }
    }
};


}
