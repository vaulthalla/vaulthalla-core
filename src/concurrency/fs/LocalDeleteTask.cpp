#include "concurrency/fs/LocalDeleteTask.hpp"
#include "storage/StorageEngine.hpp"
#include "types/fs/TrashedFile.hpp"
#include "config/ConfigRegistry.hpp"
#include "database/Queries/FileQueries.hpp"
#include "types/fs/Path.hpp"
#include "logging/LogRegistry.hpp"
#include "types/sync/ScopedOp.hpp"

using namespace vh::concurrency;
using namespace vh::types;
using namespace vh::database;
using namespace vh::config;
using namespace vh::storage;
using namespace vh::logging;

LocalDeleteTask::LocalDeleteTask(std::shared_ptr<StorageEngine> eng, std::shared_ptr<TrashedFile> f, sync::ScopedOp& op)
    : engine(std::move(eng)), file(std::move(f)), op(op) {}


void LocalDeleteTask::operator()() {
    try {
        op.start(file->size_bytes);
        auto absPath = engine->paths->absPath(file->backing_path, PathType::BACKING_ROOT);
        if (fs::exists(absPath)) fs::remove(absPath);

        while (absPath.has_parent_path()) {
            if (const auto p = absPath.parent_path();
                !fs::exists(p) || !fs::is_empty(p) || p == engine->paths->vaultRoot) break;
            fs::remove(absPath.parent_path());
            absPath = absPath.parent_path();
        }

        const auto vaultPath = engine->paths->absRelToRoot(file->backing_path, PathType::VAULT_ROOT);

        for (const auto& size : ConfigRegistry::get().caching.thumbnails.sizes)
            if (const auto thumbPath = engine->paths->absPath(vaultPath, PathType::THUMBNAIL_ROOT) / std::to_string(size);
                fs::exists(thumbPath)) fs::remove(thumbPath);

        if (const auto cachePath = engine->paths->absPath(vaultPath, PathType::CACHE_ROOT);
            fs::exists(cachePath)) fs::remove(cachePath);

        FileQueries::markTrashedFileDeleted(file->id);

        op.stop();
        op.success = true;
    } catch (const std::exception& e) {
        LogRegistry::sync()->error("[LocalDeleteTask] Failed to delete trashed file: {} - {}", file->backing_path.string(), e.what());
        op.stop();
    }

    promise.set_value(op.success);
}

