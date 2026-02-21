#include "concurrency/sync/MirrorSyncTask.hpp"
#include "storage/cloud/CloudStorageEngine.hpp"
#include "types/fs/File.hpp"
#include "types/fs/Directory.hpp"
#include "types/sync/RemotePolicy.hpp"
#include "util/fsPath.hpp"

#include <filesystem>

using namespace vh::concurrency;
using namespace vh::storage;
using namespace vh::types;

MirrorSyncTask::MirrorSyncTask(const std::shared_ptr<StorageEngine>& engine)
    : SyncTask(engine) {}

void MirrorSyncTask::sync() {
    const auto sync = std::static_pointer_cast<sync::RemotePolicy>(engine->sync);
    if (sync->conflict_policy == sync::RemotePolicy::ConflictPolicy::KeepLocal) syncKeepLocal();
    else if (sync->conflict_policy == sync::RemotePolicy::ConflictPolicy::KeepRemote) syncKeepRemote();
    else throw std::runtime_error("[MirrorSyncTask] Conflict policy not supported: " + to_string(sync->conflict_policy));
}

void MirrorSyncTask::syncKeepLocal() {
    for (const auto& file : localFiles) {
        const auto strippedPath = stripLeadingSlash(file->path).u8string();
        const auto match = s3Map.find(strippedPath);

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

        upload(file);
        s3Map.erase(match);
    }

    futures.reserve(s3Map.size());
    for (const auto& file : uMap2Vector(s3Map)) remove(file, CloudDeleteTask::Type::REMOTE);
    processFutures();
}

void MirrorSyncTask::syncKeepRemote() {
    for (const auto& file : s3Files) {
        const auto localRelPath = std::filesystem::path("/" / file->path).u8string();
        const auto match = localMap.find(localRelPath);

        if (match == localMap.end()) {
            download(file);
            continue;
        }

        const auto localFile = match->second;

        if (localFile->content_hash == cloudEngine()->getRemoteContentHash(file->path)) {
            localMap.erase(match);
            continue;
        }

        download(file);
        localMap.erase(match);
    }

    futures.reserve(localMap.size());
    for (const auto& file : uMap2Vector(localMap)) remove(file, CloudDeleteTask::Type::LOCAL);
    processFutures();
}
