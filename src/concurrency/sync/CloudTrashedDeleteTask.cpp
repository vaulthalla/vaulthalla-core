#include "concurrency/sync/CloudTrashedDeleteTask.hpp"
#include "storage/cloud/CloudStorageEngine.hpp"
#include "types/TrashedFile.hpp"
#include "types/CacheIndex.hpp"
#include "types/Path.hpp"
#include "database/Queries/FileQueries.hpp"
#include "config/ConfigRegistry.hpp"
#include "config/Config.hpp"
#include "logging/LogRegistry.hpp"

using namespace vh::concurrency;
using namespace vh::types;
using namespace vh::database;
using namespace vh::storage;
using namespace vh::config;
using namespace vh::logging;

CloudTrashedDeleteTask::CloudTrashedDeleteTask(std::shared_ptr<CloudStorageEngine> eng,
                                                 std::shared_ptr<TrashedFile> f,
                                                 const Type& type)
    : engine(std::move(eng)), file(std::move(f)), type(type) {}

void CloudTrashedDeleteTask::operator()() {
    try {
        const auto vaultPath = engine->paths->absRelToAbsRel(file->backing_path, PathType::FUSE_ROOT, PathType::VAULT_ROOT);
        if (type == Type::PURGE) purge(vaultPath);
        else if (type == Type::LOCAL) handleLocalDelete();
        else if (type == Type::REMOTE) engine->removeRemotely(vaultPath);
        else throw std::runtime_error("Unknown delete task type");
        FileQueries::markTrashedFileDeleted(file->id);
        promise.set_value(true);
    } catch (const std::exception& e) {
        LogRegistry::sync()->error("[CloudTrashedDeleteTask] Failed to delete trashed file: {} - {}", file->backing_path.string(), e.what());
        promise.set_value(false);
    }
}

void CloudTrashedDeleteTask::purge(const std::filesystem::path& path) const {
    handleLocalDelete();
    engine->removeRemotely(path);
}

void CloudTrashedDeleteTask::handleLocalDelete() const {
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
}

