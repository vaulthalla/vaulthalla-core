#include "tasks/sync/SafeSyncTask.hpp"
#include "tasks/sync/DownloadTask.hpp"
#include "concurrency/ThreadPool.hpp"
#include "database/Queries/FileQueries.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "storage/StorageManager.hpp"
#include "storage/StorageEngine.hpp"
#include "types/File.hpp"
#include "types/Directory.hpp"
#include "types/Vault.hpp"

#include <optional>

using namespace vh::concurrency;
using namespace vh::storage;
using namespace vh::types;
using namespace vh::database;

void SafeSyncTask::sync() {
    for (const auto& file : localFiles_) {
        const auto strippedPath = stripLeadingSlash(file->path);
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

    // Create any missing directories locally based on what's in S3
    for (const auto& dir : cloudEngine()->extractDirectories(uMap2Vector(s3Map_))) {
        if (!DirectoryQueries::directoryExists(engine_->vault->id, dir->path)) {
            std::cout << "[SafeSyncTask] Creating directory: " << dir->path << "\n";
            dir->parent_id = DirectoryQueries::getDirectoryIdByPath(engine_->vault->id, dir->path.parent_path());
            if (dir->abs_path.empty()) dir->abs_path = engine_->getAbsolutePath(dir->path);
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

    for (const auto& file : filesToDownload) {
        download(file);
    }

    processFutures();
}
