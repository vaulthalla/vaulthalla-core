#pragma once

#include "concurrency/sync/MirrorSyncTask.hpp"
#include "concurrency/sync/DownloadTask.hpp"
#include "concurrency/ThreadPool.hpp"
#include "database/Queries/FileQueries.hpp"
#include "storage/StorageManager.hpp"
#include "types/File.hpp"
#include "types/Directory.hpp"
#include "types/Sync.hpp"

#include <filesystem>

using namespace vh::concurrency;
using namespace vh::storage;
using namespace vh::types;
using namespace vh::database;

void MirrorSyncTask::sync() {
    if (engine_->sync->conflict_policy == Sync::ConflictPolicy::KeepLocal) syncKeepLocal();
    else if (engine_->sync->conflict_policy == Sync::ConflictPolicy::KeepRemote) syncKeepRemote();
    else throw std::runtime_error("[MirrorSyncTask] Conflict policy not supported: " + to_string(engine_->sync->conflict_policy));
}

void MirrorSyncTask::syncKeepLocal() {
    for (const auto& file : localFiles_) {
        const auto match = s3Map_.find(stripLeadingSlash(file->path));

        if (match == s3Map_.end()) {
            upload(file);
            continue;
        }

        const auto rFile = match->second;
        const auto remoteHash = std::make_optional(engine_->getRemoteContentHash(rFile->path));

        if (file->content_hash && remoteHash && *file->content_hash == remoteHash) {
            s3Map_.erase(match);
            continue;
        }

        upload(file);
        s3Map_.erase(match);
    }

    futures_.reserve(s3Map_.size());
    for (const auto& file : uMap2Vector(s3Map_)) remove(file, DeleteTask::Type::REMOTE);
    processFutures();
}

void MirrorSyncTask::syncKeepRemote() {
    for (const auto& file : s3Files_) {
        const auto localRelPath = std::filesystem::path("/" / file->path).u8string();
        const auto match = localMap_.find(localRelPath);

        if (match == localMap_.end()) {
            download(file);
            continue;
        }

        const auto localFile = match->second;

        if (localFile->content_hash == engine_->getRemoteContentHash(file->path)) {
            localMap_.erase(match);
            continue;
        }

        download(file);
        localMap_.erase(match);
    }

    futures_.reserve(localMap_.size());
    for (const auto& file : uMap2Vector(localMap_)) remove(file, DeleteTask::Type::LOCAL);
    processFutures();
}
