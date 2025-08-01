#include "concurrency/sync/SyncTask.hpp"
#include "storage/CloudStorageEngine.hpp"
#include "concurrency/sync/DownloadTask.hpp"
#include "concurrency/sync/UploadTask.hpp"
#include "types/Vault.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "database/Queries/FileQueries.hpp"
#include "database/Queries/SyncQueries.hpp"
#include "services/SyncController.hpp"
#include "types/File.hpp"
#include "types/Directory.hpp"
#include "types/Sync.hpp"

#include <iostream>
#include <utility>

using namespace vh::concurrency;
using namespace vh::types;
using namespace vh::database;
using namespace std::chrono;

void SyncTask::operator()() {
    std::cout << "[SyncWorker] Started sync for vault: " << engine_->vault->name << std::endl;
    const auto start = steady_clock::now();

    try {
        handleInterrupt();

        if (!engine_) {
            std::cerr << "[SyncWorker] Engine not initialized!" << std::endl;
            return;
        }

        isRunning_ = true;
        SyncQueries::reportSyncStarted(engine_->sync->id);

        processOperations();
        handleInterrupt();
        removeTrashedFiles();
        handleInterrupt();

        s3Map_ = cloudEngine()->getGroupedFilesFromS3();
        s3Files_ = uMap2Vector(s3Map_);
        localFiles_ = FileQueries::listFilesInDir(engine_->vault->id);
        localMap_ = groupEntriesByPath(localFiles_);

        for (const auto& [path, entry] : intersect(s3Map_, localMap_))
            remoteHashMap_.insert({entry->path.u8string(), cloudEngine()->getRemoteContentHash(entry->path)});


        futures_.clear();

        sync();

        std::cout << "[SyncWorker] Sync finished for Vault: " << engine_->vault->name << std::endl;
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
    } catch (const std::exception& e) {
        isRunning_ = false;
        if (std::string(e.what()).contains("Sync task interrupted")) {
            std::cout << "[SyncWorker] Sync task interrupted for vault: " << engine_->vault-> name << std::endl;
            return;
        }
        std::cerr << "[SyncWorker] Error during sync: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "[SyncWorker] Unknown error during sync." << std::endl;
        isRunning_ = false;
    }

    std::cout << "[SyncWorker] Sync task completed for vault: " << engine_->vault->name
              << " in " << duration_cast<milliseconds>(steady_clock::now() - start).count()
              << " ms" << std::endl;
}

void SyncTask::removeTrashedFiles() {
    const auto files = FileQueries::listTrashedFiles(vaultId());
    futures_.reserve(files.size());
    for (const auto& file : files)
        push(std::make_shared<CloudTrashedDeleteTask>(cloudEngine(), file));
    processFutures();
}

void SyncTask::upload(const std::shared_ptr<File>& file) {
    push(std::make_shared<UploadTask>(cloudEngine(), file));
}

void SyncTask::download(const std::shared_ptr<File>& file, const bool freeAfterDownload) {
    push(std::make_shared<DownloadTask>(cloudEngine(), file, freeAfterDownload));
}

void SyncTask::remove(const std::shared_ptr<File>& file, const CloudDeleteTask::Type& type) {
    push(std::make_shared<CloudDeleteTask>(cloudEngine(), file, type));
}

uintmax_t SyncTask::computeReqFreeSpaceForDownload(const std::vector<std::shared_ptr<File> >& files) {
    uintmax_t totalSize = 0;
    for (const auto& file : files) totalSize += file->size_bytes;
    return totalSize;
}

std::shared_ptr<vh::storage::CloudStorageEngine> SyncTask::cloudEngine() const { return std::static_pointer_cast<storage::CloudStorageEngine>(engine_); }

std::vector<std::shared_ptr<File> > SyncTask::uMap2Vector(
    std::unordered_map<std::u8string, std::shared_ptr<File> >& map) {

    std::vector<std::shared_ptr<File> > files;
    files.reserve(map.size());
    std::ranges::transform(map.begin(), map.end(), std::back_inserter(files),
                           [](const auto& pair) { return pair.second; });
    return files;
}

void SyncTask::ensureFreeSpace(const uintmax_t size) const {
    if (engine_->vault->quota != 0 && engine_->freeSpace() < size) throw std::runtime_error(
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

std::unordered_map<std::u8string, std::shared_ptr<File>> SyncTask::intersect(
    const std::unordered_map<std::u8string, std::shared_ptr<File> >& a,
    const std::unordered_map<std::u8string, std::shared_ptr<File> >& b) {
    std::unordered_map<std::u8string, std::shared_ptr<File>> result;

    for (const auto& item : a) if (b.contains(item.first)) result.insert(item);

    return result;
}
