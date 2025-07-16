#pragma once

#include "concurrency/sync/MirrorSyncTask.hpp"
#include "concurrency/sync/DownloadTask.hpp"
#include "concurrency/ThreadPool.hpp"
#include "database/Queries/FileQueries.hpp"
#include "storage/StorageManager.hpp"
#include "storage/StorageEngine.hpp"
#include "concurrency/thumbnail/ThumbnailWorker.hpp"
#include "types/File.hpp"
#include "types/Directory.hpp"
#include "types/Sync.hpp"

#include <filesystem>

using namespace vh::concurrency;
using namespace vh::storage;
using namespace vh::types;
using namespace vh::database;

void MirrorSyncTask::sync(std::unordered_map<std::u8string, std::shared_ptr<File> >& s3Map) const {
    if (engine_->sync->conflict_policy == Sync::ConflictPolicy::KeepLocal) syncKeepLocal(s3Map);
    else if (engine_->sync->conflict_policy == types::Sync::ConflictPolicy::KeepRemote) syncKeepRemote(s3Map);
    else throw std::runtime_error("[MirrorSyncTask] Conflict policy not supported: " + to_string(engine_->sync->conflict_policy));
}

void MirrorSyncTask::handleDiff(std::unordered_map<std::u8string, std::shared_ptr<File> >& s3Map) const {
    // s3Map may actually be localMap if we are keeping remote files
    if (engine_->sync->conflict_policy == Sync::ConflictPolicy::KeepLocal)
        for (const auto& file : uMap2Vector(s3Map)) engine_->removeRemotely(file->path);
    else if (engine_->sync->conflict_policy == Sync::ConflictPolicy::KeepRemote)
        for (const auto& file : uMap2Vector(s3Map)) engine_->removeLocally(file->path);
    else throw std::runtime_error("[MirrorSyncTask] Conflict policy not supported: " + to_string(engine_->sync->conflict_policy));
}

void MirrorSyncTask::syncKeepLocal(std::unordered_map<std::u8string, std::shared_ptr<File> >& s3Map) const {
    for (const auto& file : FileQueries::listFilesInDir(engine_->vaultId())) {
        const auto match = s3Map.find(stripLeadingSlash(file->path));

        if (match == s3Map.end()) {
            std::cout << "[MirrorSyncTask] Local file not found in S3 map, caching: " << file->path << "\n";
            engine_->uploadFile(file->path);
            continue;
        }

        const auto rFile = match->second;

        if (file->content_hash == engine_->getRemoteContentHash(rFile->path)) {
            s3Map.erase(match);
            continue;
        }

        engine_->uploadFile(file->path);
        s3Map.erase(match);
    }
}

void MirrorSyncTask::syncKeepRemote(std::unordered_map<std::u8string, std::shared_ptr<File> >& s3Map) const {
    auto localMap = groupEntriesByPath(FileQueries::listFilesInDir(engine_->vaultId()));

    for (const auto& file : uMap2Vector(s3Map)) {
        const auto localRelPath = std::filesystem::path("/" / file->path).u8string();
        const auto match = localMap.find(localRelPath);

        if (match == localMap.end()) {
            std::cout << "[MirrorSyncTask] S3 file not found in Local map, downloading: " << file->path << "\n";
            engine_->downloadFile(stripLeadingSlash(file->path));
            continue;
        }

        const auto localFile = match->second;

        if (localFile->content_hash == engine_->getRemoteContentHash(file->path)) {
            localMap.erase(match);
            continue;
        }

        engine_->downloadFile(file->path);
        localMap.erase(match);
    }

    s3Map = localMap;
}
