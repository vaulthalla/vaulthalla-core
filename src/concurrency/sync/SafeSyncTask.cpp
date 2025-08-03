#include "concurrency/sync/SafeSyncTask.hpp"
#include "concurrency/sync/DownloadTask.hpp"
#include "concurrency/ThreadPool.hpp"
#include "database/Queries/FileQueries.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "storage/StorageManager.hpp"
#include "storage/StorageEngine.hpp"
#include "types/File.hpp"
#include "types/Directory.hpp"
#include "types/Vault.hpp"
#include "types/Path.hpp"
#include "util/fsPath.hpp"

#include <optional>

#include "concurrency/ThreadPoolManager.hpp"

using namespace vh::concurrency;
using namespace vh::storage;
using namespace vh::types;
using namespace vh::database;

void SafeSyncTask::sync() {
    std::cout << "[SafeSyncTask] Starting sync for vault: " << engine_->vault->name << std::endl;
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

    for (const auto& dir : cloudEngine()->extractDirectories(uMap2Vector(s3Map_))) {
        if (!DirectoryQueries::directoryExists(engine_->vault->id, dir->path)) {
            dir->parent_id = DirectoryQueries::getDirectoryIdByPath(engine_->vault->id, dir->path.parent_path());
            if (dir->fuse_path.empty()) dir->fuse_path = engine_->paths->absPath(dir->path, PathType::VAULT_ROOT);
            DirectoryQueries::upsertDirectory(dir);
        }
    }

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

    std::cout << "[SafeSyncTask] Sync completed for vault: " << engine_->vault->name << std::endl;
}
