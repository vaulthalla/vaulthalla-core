#pragma once

#include "concurrency/Task.hpp"
#include "storage/LocalDiskStorageEngine.hpp"
#include "types/File.hpp"
#include "types/CacheIndex.hpp"
#include "config/ConfigRegistry.hpp"

#include <memory>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

namespace vh::concurrency {

struct LocalDeleteTask : PromisedTask {

    std::shared_ptr<storage::LocalDiskStorageEngine> engine;
    std::shared_ptr<types::File> file;

    LocalDeleteTask(std::shared_ptr<storage::LocalDiskStorageEngine> eng,
                 std::shared_ptr<types::File> f)
        : engine(std::move(eng)), file(std::move(f)) {}

    void operator()() override {
        try {
            const auto mount = engine->getAbsolutePath({});
            auto absPath = engine->getAbsolutePath(file->path);
            if (fs::exists(absPath)) fs::remove(absPath);

            while (absPath.has_parent_path()) {
                const auto p = absPath.parent_path();
                if (!fs::exists(p) || !fs::is_empty(p) || p == mount) break;
                fs::remove(absPath.parent_path());
                absPath = absPath.parent_path();
            }

            for (const auto& size : config::ConfigRegistry::get().caching.thumbnails.sizes) {
                const auto thumbPath = engine->getAbsoluteCachePath(file->path, fs::path("thumbnails") / std::to_string(size));
                if (fs::exists(thumbPath)) fs::remove(thumbPath);
            }

            const auto cachePath = engine->getAbsoluteCachePath(file->path, "files");
            if (fs::exists(cachePath)) fs::remove(cachePath);
            promise.set_value(true);
        } catch (const std::exception& e) {
            std::cerr << "[LocalDeleteTask] Error: " << e.what() << " (path = " << file->path << ")\n";
            promise.set_value(false);
        }
    }
};

}
