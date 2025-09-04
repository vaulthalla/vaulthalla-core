#include "concurrency/fs/LocalDeleteTask.hpp"
#include "storage/StorageEngine.hpp"
#include "types/TrashedFile.hpp"
#include "config/ConfigRegistry.hpp"
#include "database/Queries/FileQueries.hpp"
#include "types/Path.hpp"
#include "logging/LogRegistry.hpp"

using namespace vh::concurrency;
using namespace vh::types;
using namespace vh::database;
using namespace vh::config;
using namespace vh::storage;
using namespace vh::logging;

LocalDeleteTask::LocalDeleteTask(std::shared_ptr<StorageEngine> eng, std::shared_ptr<TrashedFile> f)
    : engine(std::move(eng)), file(std::move(f)) {}


void LocalDeleteTask::operator()() {
    try {
        auto absPath = engine->paths->absPath(file->backing_path, PathType::BACKING_ROOT);
        if (fs::exists(absPath)) fs::remove(absPath);

        while (absPath.has_parent_path()) {
            const auto p = absPath.parent_path();
            if (!fs::exists(p) || !fs::is_empty(p) || p == engine->paths->vaultRoot) break;
            fs::remove(absPath.parent_path());
            absPath = absPath.parent_path();
        }

        const auto vaultPath = engine->paths->absRelToRoot(file->backing_path, PathType::VAULT_ROOT);

        for (const auto& size : ConfigRegistry::get().caching.thumbnails.sizes) {
            const auto thumbPath = engine->paths->absPath(vaultPath, PathType::THUMBNAIL_ROOT) / std::to_string(size);
            if (fs::exists(thumbPath)) fs::remove(thumbPath);
        }

        const auto cachePath = engine->paths->absPath(vaultPath, PathType::CACHE_ROOT);
        if (fs::exists(cachePath)) fs::remove(cachePath);

        FileQueries::markTrashedFileDeleted(file->id);
        promise.set_value(true);
    } catch (const std::exception& e) {
        LogRegistry::sync()->error("[LocalDeleteTask] Failed to delete trashed file: {} - {}", file->backing_path.string(), e.what());
        promise.set_value(false);
    }
}

