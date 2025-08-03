#pragma once

#include "concurrency/Task.hpp"
#include "storage/CloudStorageEngine.hpp"
#include "types/TrashedFile.hpp"
#include "types/File.hpp"
#include "types/CacheIndex.hpp"
#include "types/Path.hpp"
#include "database/Queries/FileQueries.hpp"

#include <memory>

using namespace vh::types;

namespace vh::concurrency {

struct CloudDeleteTask final : PromisedTask {
    enum class Type { PURGE, LOCAL, REMOTE };

    std::shared_ptr<storage::CloudStorageEngine> engine;
    std::shared_ptr<File> file;
    Type type{Type::PURGE};

    CloudDeleteTask(std::shared_ptr<storage::CloudStorageEngine> eng,
                 std::shared_ptr<File> f,
                 const Type& type = Type::PURGE)
        : engine(std::move(eng)), file(std::move(f)), type(type) {}

    void operator()() override {
        try {
            if (type == Type::PURGE) engine->purge(file->path);
            else if (type == Type::LOCAL) engine->removeLocally(file->path);
            else if (type == Type::REMOTE) engine->removeRemotely(file->path);
            else throw std::runtime_error("Unknown delete task type");
            promise.set_value(true);
        } catch (const std::exception& e) {
            promise.set_value(false);
        }
    }
};

struct CloudTrashedDeleteTask final : PromisedTask {
    enum class Type { PURGE, LOCAL, REMOTE };

    std::shared_ptr<storage::CloudStorageEngine> engine;
    std::shared_ptr<TrashedFile> file;
    Type type{Type::PURGE};

    CloudTrashedDeleteTask(std::shared_ptr<storage::CloudStorageEngine> eng,
                 std::shared_ptr<TrashedFile> f,
                 const Type& type = Type::PURGE)
        : engine(std::move(eng)), file(std::move(f)), type(type) {}

    void operator()() override {
        try {
            const auto vaultPath = engine->paths->absRelToAbsOther(file->fuse_path, PathType::FUSE_ROOT, PathType::VAULT_ROOT);
            if (type == Type::PURGE) purge(vaultPath);
            else if (type == Type::LOCAL) handleLocalDelete();
            else if (type == Type::REMOTE) engine->removeRemotely(vaultPath);
            else throw std::runtime_error("Unknown delete task type");
            FileQueries::markTrashedFileDeleted(file->id);
            promise.set_value(true);
        } catch (const std::exception& e) {
            promise.set_value(false);
        }
    }

    void purge(const fs::path& path) const {
        handleLocalDelete();
        engine->removeRemotely(path);
    }

    void handleLocalDelete() const {
        auto absPath = engine->paths->absPath(file->fuse_path, PathType::BACKING_ROOT);
        if (fs::exists(absPath)) fs::remove(absPath);

        while (absPath.has_parent_path()) {
            const auto p = absPath.parent_path();
            if (!fs::exists(p) || !fs::is_empty(p) || p == engine->paths->vaultRoot) break;
            fs::remove(absPath.parent_path());
            absPath = absPath.parent_path();
        }

        const auto vaultPath = engine->paths->absRelToRoot(file->fuse_path, PathType::VAULT_ROOT);

        for (const auto& size : ConfigRegistry::get().caching.thumbnails.sizes) {
            const auto thumbPath = engine->paths->absPath(vaultPath, PathType::THUMBNAIL_ROOT) / std::to_string(size);
            if (fs::exists(thumbPath)) fs::remove(thumbPath);
        }

        const auto cachePath = engine->paths->absPath(vaultPath, PathType::CACHE_ROOT);
        if (fs::exists(cachePath)) fs::remove(cachePath);
    }
};

}
