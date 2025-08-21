#include "concurrency/sync/MirrorSyncTask.hpp"
#include "concurrency/ThreadPool.hpp"
#include "database/Queries/FileQueries.hpp"
#include "storage/StorageManager.hpp"
#include "storage/CloudStorageEngine.hpp"
#include "types/File.hpp"
#include "types/Directory.hpp"
#include "types/RSync.hpp"
#include "util/fsPath.hpp"

#include <filesystem>

using namespace vh::concurrency;
using namespace vh::storage;
using namespace vh::types;
using namespace vh::database;

MirrorSyncTask::MirrorSyncTask(const std::shared_ptr<StorageEngine>& engine)
    : SyncTask(engine) {}

void MirrorSyncTask::sync() {
    const auto sync = std::static_pointer_cast<RSync>(engine_->sync);
    if (sync->conflict_policy == RSync::ConflictPolicy::KeepLocal) syncKeepLocal();
    else if (sync->conflict_policy == RSync::ConflictPolicy::KeepRemote) syncKeepRemote();
    else throw std::runtime_error("[MirrorSyncTask] Conflict policy not supported: " + to_string(sync->conflict_policy));
}

void MirrorSyncTask::syncKeepLocal() {
    for (const auto& file : localFiles_) {
        const auto strippedPath = stripLeadingSlash(file->path).u8string();
        const auto match = s3Map_.find(strippedPath);

        if (match == s3Map_.end()) {
            upload(file);
            continue;
        }

        if (file->content_hash && remoteHashMap_[strippedPath] && *file->content_hash == remoteHashMap_[strippedPath]) {
            s3Map_.erase(match);
            continue;
        }

        upload(file);
        s3Map_.erase(match);
    }

    futures_.reserve(s3Map_.size());
    for (const auto& file : uMap2Vector(s3Map_)) remove(file, CloudDeleteTask::Type::REMOTE);
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

        if (localFile->content_hash == cloudEngine()->getRemoteContentHash(file->path)) {
            localMap_.erase(match);
            continue;
        }

        download(file);
        localMap_.erase(match);
    }

    futures_.reserve(localMap_.size());
    for (const auto& file : uMap2Vector(localMap_)) remove(file, CloudDeleteTask::Type::LOCAL);
    processFutures();
}
