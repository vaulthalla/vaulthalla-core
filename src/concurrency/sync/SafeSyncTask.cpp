#include "concurrency/sync/SafeSyncTask.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "storage/CloudStorageEngine.hpp"
#include "types/File.hpp"
#include "types/Directory.hpp"
#include "types/Vault.hpp"
#include "types/Path.hpp"
#include "util/fsPath.hpp"
#include "services/LogRegistry.hpp"
#include "crypto/IdGenerator.hpp"

#include <optional>

using namespace vh::concurrency;
using namespace vh::storage;
using namespace vh::types;
using namespace vh::database;
using namespace vh::logging;

SafeSyncTask::SafeSyncTask(const std::shared_ptr<CloudStorageEngine>& engine)
    : SyncTask(engine) {}

void SafeSyncTask::sync() {
    LogRegistry::sync()->info("[SafeSyncTask] Starting sync for vault '{}'", engine_->vault->id);

    for (const auto& dir : cloudEngine()->extractDirectories(uMap2Vector(s3Map_))) {
        if (!DirectoryQueries::directoryExists(engine_->vault->id, dir->path)) {
            dir->parent_id = DirectoryQueries::getDirectoryIdByPath(engine_->vault->id, dir->path.parent_path());
            if (dir->fuse_path.empty()) dir->fuse_path = engine_->paths->absPath(dir->path, PathType::VAULT_ROOT);
            dir->base32_alias = ids::IdGenerator({ .namespace_token = dir->name }).generate();
            DirectoryQueries::upsertDirectory(dir);
        }
    }

    for (const auto& file : localFiles_) {
        const auto strippedPath = stripLeadingSlash(file->path).u8string();
        auto match = s3Map_.find(strippedPath);

        if (match == s3Map_.end()) {
            upload(file);
            continue;
        }

        if (file->content_hash && remoteHashMap_[strippedPath] && *file->content_hash == remoteHashMap_[strippedPath]) {
            s3Map_.erase(match);
            continue;
        }

        if (file->updated_at <= match->second->updated_at) download(file);
        else upload(file);

        s3Map_.erase(match);
    }

    processFutures();

    const auto filesToDownload = uMap2Vector(s3Map_);
    futures_.reserve(filesToDownload.size());

    const auto requiredSpace = computeReqFreeSpaceForDownload(filesToDownload);
    const auto availableSpace = engine_->freeSpace();

    if (availableSpace < requiredSpace) {
        throw std::runtime_error(
            "[SafeSyncTask] Not enough free space for download. Required: " +
            std::to_string(requiredSpace) + ", Available: " + std::to_string(availableSpace));
    }

    for (const auto& file : filesToDownload) download(file);

    processFutures();

    LogRegistry::sync()->info("[SafeSyncTask] Sync completed for vault '{}' with {} files processed",
                              engine_->vault->id, localFiles_.size() + s3Map_.size());
}
