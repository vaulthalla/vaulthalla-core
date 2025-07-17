#pragma once

#include "concurrency/sync/SafeSyncTask.hpp"
#include "concurrency/sync/DownloadTask.hpp"
#include "concurrency/ThreadPool.hpp"
#include "database/Queries/FileQueries.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "storage/StorageManager.hpp"
#include "storage/StorageEngine.hpp"
#include "types/File.hpp"
#include "types/Directory.hpp"

#include <optional>

using namespace vh::concurrency;
using namespace vh::storage;
using namespace vh::types;
using namespace vh::database;

void SafeSyncTask::sync() {
    for (const auto& file : localFiles_) {
        const auto strippedPath = stripLeadingSlash(file->path);
        auto matchIter = s3Map_.find(strippedPath);

        if (matchIter == s3Map_.end()) {
            upload(file);
            continue;
        }

        const auto& remoteFile = matchIter->second;
        const auto remoteHash = std::make_optional(engine_->getRemoteContentHash(remoteFile->path));

        if (remoteHash && file->content_hash == remoteHash) {
            s3Map_.erase(matchIter);
            continue;
        }

        if (file->updated_at <= remoteFile->updated_at) {
            download(file);
        } else {
            upload(file);
        }

        s3Map_.erase(matchIter);
    }

    processFutures();

    // Create any missing directories locally based on what's in S3
    for (const auto& dir : engine_->extractDirectories(uMap2Vector(s3Map_))) {
        if (!DirectoryQueries::directoryExists(engine_->vaultId(), dir->path)) {
            std::cout << "[SafeSyncTask] Creating directory: " << dir->path << "\n";
            dir->parent_id = DirectoryQueries::getDirectoryIdByPath(engine_->vaultId(), dir->path.parent_path());
            DirectoryQueries::addDirectory(dir);
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
