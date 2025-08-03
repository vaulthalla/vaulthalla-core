#pragma once

#include "concurrency/Task.hpp"
#include "storage/CloudStorageEngine.hpp"
#include "types/File.hpp"

#include <memory>

namespace vh::concurrency {

struct DownloadTask : PromisedTask {
    std::shared_ptr<storage::CloudStorageEngine> engine;
    std::shared_ptr<File> file;
    bool freeAfterDownload;

    DownloadTask(std::shared_ptr<storage::CloudStorageEngine> eng,
                 std::shared_ptr<File> f,
                 const bool freeAfter = false)
        : engine(std::move(eng)), file(std::move(f)), freeAfterDownload(freeAfter) {}

    void operator()() override {
        try {
            if (freeAfterDownload) engine->indexAndDeleteFile(file->path);
            else engine->downloadFile(file->path);
            promise.set_value(true);
        } catch (const std::exception& e) {
            promise.set_value(false); // or wrap exception if desired
        }
    }
};

}
