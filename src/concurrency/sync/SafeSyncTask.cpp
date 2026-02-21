#include "concurrency/sync/SafeSyncTask.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "storage/cloud/CloudStorageEngine.hpp"
#include "types/fs/File.hpp"
#include "types/fs/Directory.hpp"
#include "types/vault/Vault.hpp"
#include "types/fs/Path.hpp"
#include "util/fsPath.hpp"
#include "logging/LogRegistry.hpp"
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
    LogRegistry::sync()->info("[SafeSyncTask] Starting sync for vault '{}'", engine->vault->id);

    for (const auto& dir : cloudEngine()->extractDirectories(uMap2Vector(s3Map))) {
        if (!DirectoryQueries::directoryExists(engine->vault->id, dir->path)) {
            dir->parent_id = DirectoryQueries::getDirectoryIdByPath(engine->vault->id, dir->path.parent_path());
            if (dir->fuse_path.empty()) dir->fuse_path = engine->paths->absPath(dir->path, PathType::VAULT_ROOT);
            dir->base32_alias = ids::IdGenerator({ .namespace_token = dir->name }).generate();
            DirectoryQueries::upsertDirectory(dir);
        }
    }

    for (const auto& file : localFiles) {
        const auto strippedPath = stripLeadingSlash(file->path).u8string();
        auto match = s3Map.find(strippedPath);

        if (match == s3Map.end()) {
            upload(file);
            continue;
        }

        const auto rFile = match->second;
        rFile->content_hash = std::make_optional(cloudEngine()->getRemoteContentHash(rFile->path));

        if (conflict(file, rFile)) continue;

        if (file->content_hash && remoteHashMap[strippedPath] && *file->content_hash == remoteHashMap[strippedPath]) {
            s3Map.erase(match);
            continue;
        }

        if (file->updated_at <= match->second->updated_at) download(file);
        else upload(file);

        s3Map.erase(match);
    }

    processFutures();

    const auto filesToDownload = uMap2Vector(s3Map);
    futures.reserve(filesToDownload.size());

    const auto requiredSpace = computeReqFreeSpaceForDownload(filesToDownload);
    const auto availableSpace = engine->freeSpace();

    if (availableSpace < requiredSpace) {
        event->error_code = "Insufficient Disk Space";
        event->error_message = "Not enough free space for download. Required: " + std::to_string(requiredSpace) + ", Available: " + std::to_string(availableSpace);
        event->stall_reason = event->error_code;
        throw std::runtime_error(
            "[SafeSyncTask] Not enough free space for download. Required: " +
            std::to_string(requiredSpace) + ", Available: " + std::to_string(availableSpace));
    }

    for (const auto& file : filesToDownload) download(file);

    processFutures();

    LogRegistry::sync()->info("[SafeSyncTask] Sync completed for vault '{}' with {} files processed",
                              engine->vault->id, localFiles.size() + s3Map.size());
}
