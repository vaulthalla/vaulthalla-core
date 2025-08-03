#pragma once

#include "concurrency/Task.hpp"
#include "storage/CloudStorageEngine.hpp"
#include "types/File.hpp"
#include "types/CacheIndex.hpp"

#include <memory>
#include <iostream>

namespace vh::concurrency {

struct UploadTask : PromisedTask {
    std::shared_ptr<storage::CloudStorageEngine> engine;
    std::shared_ptr<File> file;

    UploadTask(std::shared_ptr<storage::CloudStorageEngine> eng,
                 std::shared_ptr<File> f)
        : engine(std::move(eng)), file(std::move(f)) {}

    void operator()() override {
        try {
            engine->uploadFile(file->path);
            promise.set_value(true);
        } catch (const std::exception& e) {
            std::cerr << "[UploadTask] Error: " << e.what() << " (path = " << file->path << ")" << std::endl;
            promise.set_value(false);
        }
    }
};

}
