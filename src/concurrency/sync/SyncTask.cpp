#include "concurrency/sync/SyncTask.hpp"
#include "storage/cloud/CloudStorageEngine.hpp"
#include "concurrency/sync/DownloadTask.hpp"
#include "concurrency/sync/UploadTask.hpp"
#include "concurrency/sync/CloudDeleteTask.hpp"
#include "concurrency/sync/CloudTrashedDeleteTask.hpp"
#include "concurrency/sync/CloudRotateKeyTask.hpp"
#include "types/vault/Vault.hpp"
#include "database/Queries/FileQueries.hpp"
#include "database/Queries/SyncQueries.hpp"
#include "types/fs/File.hpp"
#include "types/sync/Sync.hpp"
#include "logging/LogRegistry.hpp"
#include "types/sync/Event.hpp"
#include "types/sync/Throughput.hpp"

#include <utility>

using namespace vh::concurrency;
using namespace vh::types;
using namespace vh::database;
using namespace std::chrono;
using namespace vh::logging;
using namespace vh::storage;

void SyncTask::operator()() {
    LogRegistry::sync()->info("[SyncTask] Preparing to sync vault '{}'", engine_->vault->id);

    if (!engine_) {
        LogRegistry::sync()->error("[SyncTask] Engine is null, cannot proceed with sync.");
        return;
    }

    newEvent();
    event_ = engine_->latestSyncEvent;
    event_->start();

    try {
        handleInterrupt();

        isRunning_ = true;
        SyncQueries::reportSyncStarted(engine_->sync->id);

        processOperations();
        handleInterrupt();
        removeTrashedFiles();
        handleInterrupt();
        handleVaultKeyRotation();
        handleInterrupt();

        s3Map_ = cloudEngine()->getGroupedFilesFromS3();
        s3Files_ = uMap2Vector(s3Map_);
        localFiles_ = FileQueries::listFilesInDir(engine_->vault->id);
        localMap_ = groupEntriesByPath(localFiles_);

        for (const auto& [path, entry] : intersect(s3Map_, localMap_))
            remoteHashMap_.insert({entry->path.u8string(), cloudEngine()->getRemoteContentHash(entry->path)});


        futures_.clear();

        sync();

        SyncQueries::reportSyncSuccess(engine_->sync->id);
        next_run = system_clock::now() + seconds(engine_->sync->interval.count());

        handleInterrupt();

        localFiles_.clear();
        s3Files_.clear();
        s3Map_.clear();
        localMap_.clear();
        remoteHashMap_.clear();

        requeue();
        isRunning_ = false;

        LogRegistry::sync()->debug("[SyncTask] Sync task requeued for vault '{}'", engine_->vault->id);
    } catch (const std::exception& e) {
        isRunning_ = false;
        if (std::string(e.what()).contains("Sync task interrupted")) {
            LogRegistry::sync()->info("[SyncTask] Sync task interrupted for vault '{}'", engine_->vault->id);
            return;
        }
        LogRegistry::sync()->error("[SyncTask] Exception during sync for vault '{}': {}", engine_->vault->id, e.what());
    } catch (...) {
        LogRegistry::sync()->error("[SyncTask] Unknown exception during sync for vault '{}'", engine_->vault->id);
        isRunning_ = false;
    }

    event_->stop();
    LogRegistry::sync()->info("[SyncTask] Sync completed for vault '{}' in {}s",
                              engine_->vault->id, event_->durationSeconds());
}

void SyncTask::pushKeyRotationTask(const std::vector<std::shared_ptr<File> >& files, unsigned int begin, unsigned int end) {
    push(std::make_shared<CloudRotateKeyTask>(cloudEngine(), files, begin, end));
}

void SyncTask::removeTrashedFiles() {
    const auto files = FileQueries::listTrashedFiles(vaultId());
    futures_.reserve(files.size());

    auto& op = event_->getOrCreateThroughput(sync::Throughput::Metric::DELETE).newOp();

    for (const auto& file : files)
        push(std::make_shared<CloudTrashedDeleteTask>(cloudEngine(), file, op));
    processFutures();
}

void SyncTask::upload(const std::shared_ptr<File>& file) {
    auto& op = event_->getOrCreateThroughput(sync::Throughput::Metric::UPLOAD).newOp();
    push(std::make_shared<UploadTask>(cloudEngine(), file, op));
}

void SyncTask::download(const std::shared_ptr<File>& file, const bool freeAfterDownload) {
    auto& op = event_->getOrCreateThroughput(sync::Throughput::Metric::DOWNLOAD).newOp();
    push(std::make_shared<DownloadTask>(cloudEngine(), file, op, freeAfterDownload));
}

void SyncTask::remove(const std::shared_ptr<File>& file, const CloudDeleteTask::Type& type) {
    auto& op = event_->getOrCreateThroughput(sync::Throughput::Metric::DELETE).newOp();
    push(std::make_shared<CloudDeleteTask>(cloudEngine(), file, op, type));
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
