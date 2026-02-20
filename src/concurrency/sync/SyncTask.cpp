#include "concurrency/sync/SyncTask.hpp"
#include "storage/cloud/CloudStorageEngine.hpp"
#include "concurrency/sync/DownloadTask.hpp"
#include "concurrency/sync/UploadTask.hpp"
#include "concurrency/sync/CloudDeleteTask.hpp"
#include "concurrency/sync/CloudTrashedDeleteTask.hpp"
#include "concurrency/sync/CloudRotateKeyTask.hpp"
#include "types/vault/Vault.hpp"
#include "database/Queries/FileQueries.hpp"
#include "types/fs/File.hpp"
#include "logging/LogRegistry.hpp"
#include "types/sync/Event.hpp"
#include "types/sync/Throughput.hpp"
#include "types/sync/Artifact.hpp"
#include "types/sync/ConflictArtifact.hpp"
#include "types/sync/Conflict.hpp"
#include "types/sync/FSync.hpp"
#include "types/sync/RSync.hpp"

#include <utility>

using namespace vh::concurrency;
using namespace vh::types;
using namespace vh::database;
using namespace std::chrono;
using namespace vh::logging;
using namespace vh::storage;

void SyncTask::operator()() {
    startTask();

    const Stage stages[] = {
        {"shared",   [this]{ processSharedOps(); }},
        {"initBins", [this]{ initBins(); }},
        {"sync",     [this]{ sync(); }},
        {"clearBins",[this]{ clearBins(); }},
    };

    runStages(stages);
    shutdown();
}

void SyncTask::initBins() {
    s3Map_ = cloudEngine()->getGroupedFilesFromS3();
    s3Files_ = uMap2Vector(s3Map_);
    localFiles_ = FileQueries::listFilesInDir(engine_->vault->id);
    localMap_ = groupEntriesByPath(localFiles_);

    event_->heartbeat();

    for (const auto& [path, entry] : intersect(s3Map_, localMap_))
        remoteHashMap_.insert({entry->path.u8string(), cloudEngine()->getRemoteContentHash(entry->path)});
}

void SyncTask::pushKeyRotationTask(const std::vector<std::shared_ptr<File> >& files, unsigned int begin, unsigned int end) {
    push(std::make_shared<CloudRotateKeyTask>(cloudEngine(), files, begin, end));
}

void SyncTask::removeTrashedFiles() {
    const auto files = FileQueries::listTrashedFiles(vaultId());

    futures_.reserve(files.size());
    for (const auto& file : files)
        push(std::make_shared<CloudTrashedDeleteTask>(cloudEngine(), file, op(sync::Throughput::Metric::DELETE)));

    processFutures();
}

void SyncTask::upload(const std::shared_ptr<File>& file) {
    push(std::make_shared<UploadTask>(cloudEngine(), file, op(sync::Throughput::Metric::UPLOAD)));
}

void SyncTask::download(const std::shared_ptr<File>& file, const bool freeAfterDownload) {
    push(std::make_shared<DownloadTask>(cloudEngine(), file, event_->getOrCreateThroughput(sync::Throughput::Metric::DOWNLOAD).newOp(), freeAfterDownload));
}

void SyncTask::remove(const std::shared_ptr<File>& file, const CloudDeleteTask::Type& type) {
    push(std::make_shared<CloudDeleteTask>(cloudEngine(), file, op(sync::Throughput::Metric::DELETE), type));
}

bool SyncTask::hasPotentialConflict(const std::shared_ptr<File>& local, const std::shared_ptr<File>& upstream, bool upstream_decryption_failure) {
    if (upstream_decryption_failure) return true;

    if (local->size_bytes != upstream->size_bytes) return true;

    if (local->content_hash && upstream->content_hash && *local->content_hash != *upstream->content_hash) return true;

    return false;
}


bool SyncTask::conflict(const std::shared_ptr<File>& local, const std::shared_ptr<File>& upstream, const bool upstream_decryption_failure) const {
    if (!hasPotentialConflict(local, upstream, upstream_decryption_failure)) return false;

    const auto conflict = std::make_shared<sync::Conflict>();
    conflict->failed_to_decrypt_upstream = upstream_decryption_failure;
    conflict->artifacts.local = sync::Artifact(local, sync::Artifact::Side::LOCAL);
    conflict->artifacts.upstream = sync::Artifact(upstream, sync::Artifact::Side::UPSTREAM);
    conflict->analyze();

    if (conflict->reasons.empty()) {
        LogRegistry::sync()->debug("[SyncTask] hasPotentialConflict() returned true, but no reasons were identified for the conflict. This might indicate a logic error in the conflict detection algorithm.");
        return false;
    }

    conflict->created_at = system_clock::to_time_t(system_clock::now());
    conflict->file_id = local->id;
    conflict->event_id = event_->id;

    const bool ok = engine()->sync->resolve_conflict(conflict);

    if (ok) conflict->resolved_at = system_clock::to_time_t(system_clock::now());
    event_->conflicts.push_back(conflict);

    return !ok;
}

uintmax_t SyncTask::computeReqFreeSpaceForDownload(const std::vector<std::shared_ptr<File> >& files) {
    uintmax_t totalSize = 0;
    for (const auto& file : files) totalSize += file->size_bytes;
    return totalSize;
}

std::shared_ptr<CloudStorageEngine> SyncTask::cloudEngine() const { return std::static_pointer_cast<storage::CloudStorageEngine>(engine_); }

std::vector<std::shared_ptr<File> > SyncTask::uMap2Vector(
    std::unordered_map<std::u8string, std::shared_ptr<File> >& map) {

    std::vector<std::shared_ptr<File> > files;
    files.reserve(map.size());
    std::ranges::transform(map.begin(), map.end(), std::back_inserter(files),
                           [](const auto& pair) { return pair.second; });
    return files;
}

void SyncTask::ensureFreeSpace(const uintmax_t size) const {
    if (engine_->vault->quota != 0 && engine_->freeSpace() < size)
        throw std::runtime_error("Not enough space to cache file");
}

std::unordered_map<std::u8string, std::shared_ptr<File>> SyncTask::symmetric_diff(
    const std::unordered_map<std::u8string, std::shared_ptr<File> >& a,
    const std::unordered_map<std::u8string, std::shared_ptr<File> >& b) {
    std::unordered_map<std::u8string, std::shared_ptr<File>> result;

    for (const auto& item : a) if (!b.contains(item.first)) result.insert(item);
    for (const auto& item : b) if (!a.contains(item.first)) result.insert(item);

    return result;
}

std::unordered_map<std::u8string, std::shared_ptr<File>> SyncTask::intersect(
    const std::unordered_map<std::u8string, std::shared_ptr<File> >& a,
    const std::unordered_map<std::u8string, std::shared_ptr<File> >& b) {
    std::unordered_map<std::u8string, std::shared_ptr<File>> result;

    for (const auto& item : a) if (b.contains(item.first)) result.insert(item);

    return result;
}

void SyncTask::clearBins() {
    localFiles_.clear();
    s3Files_.clear();
    s3Map_.clear();
    localMap_.clear();
    remoteHashMap_.clear();
}
