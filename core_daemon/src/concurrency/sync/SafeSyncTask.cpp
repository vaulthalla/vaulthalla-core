#pragma once

#include "concurrency/sync/SafeSyncTask.hpp"
#include "concurrency/sync/DownloadTask.hpp"
#include "concurrency/ThreadPool.hpp"
#include "concurrency/ThreadPoolRegistry.hpp"
#include "database/Queries/FileQueries.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "database/Queries/CacheQueries.hpp"
#include "storage/StorageManager.hpp"
#include "storage/StorageEngine.hpp"
#include "services/ThumbnailWorker.hpp"
#include "types/File.hpp"
#include "types/Directory.hpp"

#include <optional>

using namespace vh::concurrency;
using namespace vh::storage;
using namespace vh::types;
using namespace vh::database;

void SafeSyncTask::sync(std::unordered_map<std::u8string, std::shared_ptr<File> >& s3Map) const {
    for (const auto& file : FileQueries::listFilesInDir(engine_->vaultId())) {
        const auto match = s3Map.find(stripLeadingSlash(file->path));

        if (match == s3Map.end()) {
            std::cout << "[SafeSyncTask] Local file not found in S3 map, caching: " << file->path << "\n";
            engine_->uploadFile(file->path);
            continue;
        }

        const auto rFile = match->second;

        if (file->content_hash == engine_->getRemoteContentHash(rFile->path)) {
            s3Map.erase(match);
            continue;
        }

        if (file->updated_at <= rFile->updated_at) {
            engine_->downloadFile(file->path);
            s3Map.erase(match);
            continue;
        }

        std::cout << "[SafeSyncTask][1/2] Local file is newer than remote copy: " << file->path << "\n"
            << "[SafeSyncTask][2/2] Assuming an upload is scheduled, skipping download.\n";

        s3Map.erase(match);
    }
}

void SafeSyncTask::handleDiff(std::unordered_map<std::u8string, std::shared_ptr<File> >& s3Map) const {
    for (const auto& dir : engine_->extractDirectories(uMap2Vector(s3Map))) {
        if (!DirectoryQueries::directoryExists(engine_->vaultId(), dir->path)) {
            std::cout << "[SafeSyncTask] Creating directory: " << dir->path << "\n";
            dir->parent_id = DirectoryQueries::getDirectoryIdByPath(engine_->vaultId(), dir->path.parent_path());
            DirectoryQueries::addDirectory(dir);
        }
    }

    const auto files = uMap2Vector(s3Map);
    std::vector<std::future<ExpectedFuture>> futures;

    const auto freeSpace = engine_->freeSpace();
    const auto requiredSpace = computeReqFreeSpaceForDownload(files);

    if (freeSpace < requiredSpace) throw std::runtime_error(
        "[SafeSyncTask] Not enough free space for download, required: " + std::to_string(requiredSpace) +
        ", available: " + std::to_string(freeSpace));

    const auto threadPool = ThreadPoolRegistry::instance().syncPool();
    for (const auto& file : files) {
        auto task = std::make_shared<DownloadTask>(engine_, file);
        futures.push_back(task->getFuture().value());
        threadPool->submit(task);
    }

    for (auto& f : futures)
        if (std::get<bool>(f.get()) == false) std::cerr << "[SafeSyncTask] A file sync task failed.\n";
}
