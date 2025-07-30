#pragma once

#include "concurrency/Task.hpp"
#include "storage/StorageEngine.hpp"
#include "types/File.hpp"
#include "types/CacheIndex.hpp"
#include "types/Path.hpp"
#include "config/ConfigRegistry.hpp"

#include <memory>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

using namespace vh::types;

namespace vh::concurrency {

struct LocalDeleteTask : PromisedTask {

    std::shared_ptr<storage::StorageEngine> engine;
    std::shared_ptr<File> file;

    LocalDeleteTask(std::shared_ptr<storage::StorageEngine> eng,
                 std::shared_ptr<File> f)
        : engine(std::move(eng)), file(std::move(f)) {}

    void operator()() override {
        try {
            auto absPath = engine->paths->absPath(file->path, PathType::BACKING_ROOT);
            if (fs::exists(absPath)) fs::remove(absPath);

            while (absPath.has_parent_path()) {
                const auto p = absPath.parent_path();
                if (!fs::exists(p) || !fs::is_empty(p) || p == engine->paths->vaultRoot) break;
                fs::remove(absPath.parent_path());
                absPath = absPath.parent_path();
            }

            for (const auto& size : ConfigRegistry::get().caching.thumbnails.sizes) {
                const auto thumbPath = engine->paths->absPath(file->path, PathType::THUMBNAIL_ROOT) / std::to_string(size);
                if (fs::exists(thumbPath)) fs::remove(thumbPath);
            }

            const auto cachePath = engine->paths->absPath("files", PathType::CACHE_ROOT);
            if (fs::exists(cachePath)) fs::remove(cachePath);
            promise.set_value(true);
        } catch (const std::exception& e) {
            std::cerr << "[LocalDeleteTask] Error: " << e.what() << " (path = " << file->path << ")\n";
            promise.set_value(false);
        }
    }
};

}
