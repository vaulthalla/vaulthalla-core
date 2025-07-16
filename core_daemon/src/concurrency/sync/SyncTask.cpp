#include "concurrency/sync/SyncTask.hpp"
#include "storage/CloudStorageEngine.hpp"
#include "types/Vault.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "database/Queries/SyncQueries.hpp"
#include "services/SyncController.hpp"
#include "types/File.hpp"
#include "types/Directory.hpp"
#include "types/Sync.hpp"
#include <iostream>
#include <filesystem>

using namespace vh::concurrency;
using namespace vh::types;

SyncTask::SyncTask(const std::shared_ptr<storage::CloudStorageEngine>& engine,
                   const std::shared_ptr<services::SyncController>& controller)
    : next_run(std::chrono::system_clock::from_time_t(engine->sync->last_sync_at)
               + std::chrono::seconds(engine->sync->interval.count())),
      engine_(engine),
      controller_(controller) {
}

void SyncTask::operator()() {
    if (!engine_) {
        std::cerr << "[SyncWorker] Engine not initialized!\n";
        return;
    }

    bool active; {
        std::shared_lock lock(controller_->engineMapMutex_);
        active = engine_->sync->enabled && controller_->engineMap_.contains(engine_->vault_->id);
    }

    if (!active) {
        std::cout << "[SyncWorker] Engine missing from map for vault ID: " << engine_->vault_->id << "\n";
        return;
    }

    std::cout << "[SyncWorker] Sync start: " << engine_->vault_->name << "\n";
    database::SyncQueries::reportSyncStarted(engine_->sync->id);

    if (!database::DirectoryQueries::directoryExists(engine_->vault_->id, "/")) {
        auto dir = std::make_shared<Directory>();
        dir->vault_id = engine_->vault_->id;
        dir->name = "/";
        dir->path = "/";
        dir->created_by = dir->last_modified_by = engine_->vault_->owner_id;
        dir->parent_id = std::nullopt;
        database::DirectoryQueries::addDirectory(dir);
    }

    auto s3Map = engine_->getGroupedFilesFromS3();

    sync(s3Map);
    handleDiff(s3Map);

    std::cout << "[SyncWorker] Sync done: " << engine_->vault_->name << "\n";
    database::SyncQueries::reportSyncSuccess(engine_->sync->id);
    next_run = std::chrono::system_clock::now() + std::chrono::seconds(engine_->sync->interval.count());
    controller_->requeue(shared_from_this());
}

uintmax_t SyncTask::computeReqFreeSpaceForDownload(const std::vector<std::shared_ptr<File> >& files) {
    uintmax_t totalSize = 0;
    for (const auto& file : files) totalSize += file->size_bytes;
    return totalSize;
}

unsigned int SyncTask::vaultId() const { return engine_->vault_->id; }

bool SyncTask::operator<(const SyncTask& other) const { return next_run > other.next_run; }

std::vector<std::shared_ptr<File> > SyncTask::uMap2Vector(
    std::unordered_map<std::u8string, std::shared_ptr<File> >& map) {

    std::vector<std::shared_ptr<File> > files;
    files.reserve(map.size());
    std::ranges::transform(map.begin(), map.end(), std::back_inserter(files),
                           [](const auto& pair) { return pair.second; });
    return files;
}

void SyncTask::ensureFreeSpace(const uintmax_t size) const {
    if (engine_->getVault()->quota != 0 && engine_->freeSpace() < size) throw std::runtime_error(
        "Not enough space to cache file");
}

std::unordered_map<std::u8string, std::shared_ptr<File>> SyncTask::symmetric_diff(
    const std::unordered_map<std::u8string, std::shared_ptr<File> >& a,
    const std::unordered_map<std::u8string, std::shared_ptr<File> >& b) {
    std::unordered_map<std::u8string, std::shared_ptr<File>> result;

    for (const auto& item : a) if (!b.contains(item.first)) result.insert(item);
    for (const auto& item : b) if (!a.contains(item.first)) result.insert(item);

    return result;
}
