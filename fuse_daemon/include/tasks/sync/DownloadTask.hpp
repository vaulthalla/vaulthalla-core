#pragma once

#include "../Task.hpp"
#include "shared/include/engine/CloudStorageEngine.hpp"
#include "shared/include/engine/StorageEngine.hpp"
#include "types/File.hpp"
#include "types/CacheIndex.hpp"

#include <memory>
#include <iostream>

namespace vh::concurrency {

struct DownloadTask : PromisedTask {
    std::shared_ptr<storage::CloudStorageEngine> engine;
    std::shared_ptr<types::File> file;
    bool freeAfterDownload;

    DownloadTask(std::shared_ptr<storage::CloudStorageEngine> eng,
                 std::shared_ptr<types::File> f,
                 const bool freeAfter = false)
        : engine(std::move(eng)), file(std::move(f)), freeAfterDownload(freeAfter) {}

    void operator()() override {
        try {
            if (freeAfterDownload) engine->indexAndDeleteFile(file->path);
            else engine->downloadFile(file->path);
            promise.set_value(true);
        } catch (const std::exception& e) {
            std::cerr << "[DownloadTask] Error: " << e.what() << " (path = " << file->path << ")\n";
            promise.set_value(false); // or wrap exception if desired
        }
    }
};

}
