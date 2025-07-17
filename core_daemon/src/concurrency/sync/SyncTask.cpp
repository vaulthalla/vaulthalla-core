#include "concurrency/sync/SyncTask.hpp"
#include "concurrency/ThreadPoolRegistry.hpp"
#include "concurrency/ThreadPool.hpp"
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
    try {
        handleInterrupt();

        if (!engine_) {
            std::cerr << "[SyncWorker] Engine not initialized!\n";
            return;
        }

        std::cout << "[SyncWorker] Sync start: " << engine_->vault_->name << "\n";
        isRunning_ = true;
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

        removeTrashedFiles();
        handleInterrupt();

        s3Map_ = engine_->getGroupedFilesFromS3();
        s3Files_ = uMap2Vector(s3Map_);
        localFiles_ = database::FileQueries::listFilesInDir(engine_->vaultId());
        localMap_ = groupEntriesByPath(localFiles_);

        futures_.clear();
        futures_.reserve(std::max(s3Map_.size(), localMap_.size()));

        sync();

        std::cout << "[SyncWorker] Sync done: " << engine_->vault_->name << "\n";
        database::SyncQueries::reportSyncSuccess(engine_->sync->id);
        next_run = std::chrono::system_clock::now() + std::chrono::seconds(engine_->sync->interval.count());

        handleInterrupt();
        controller_->requeue(shared_from_this());
        isRunning_ = false;
    } catch (const std::exception& e) {
        isRunning_ = false;
        if (std::string(e.what()).contains("Sync task interrupted")) {
            std::cout << "[SyncWorker] Sync task interrupted for vault ID: " << engine_->vault_->id << "\n";
            return;
        }
        std::cerr << "[SyncWorker] Error during sync: " << e.what() << "\n";
    } catch (...) {
        std::cerr << "[SyncWorker] Unknown error during sync.\n";
        isRunning_ = false;
    }
}

void SyncTask::removeTrashedFiles() {
    const auto files = database::FileQueries::listTrashedFiles(vaultId());
    futures_.reserve(files.size());
    std::cout << "[SyncWorker] Removing " << files.size() << " trashed files from vault ID: " << vaultId() << std::endl;

    for (const auto& file : files) remove(file);

    for (const auto& dir : database::DirectoryQueries::listTrashedDirs(vaultId()))
        database::DirectoryQueries::deleteDirectory(dir->id);

    processFutures();
}

void SyncTask::processFutures() {
    for (auto& f : futures_)
        if (std::get<bool>(f.get()) == false)
            std::cerr << "[SafeSyncTask] A file sync task failed.\n";
    futures_.clear();
}


void SyncTask::push(const std::shared_ptr<Task>& task) {
    futures_.push_back(task->getFuture().value());
    ThreadPoolRegistry::instance().syncPool()->submit(task);
}

void SyncTask::upload(const std::shared_ptr<File>& file) {
    push(std::make_shared<UploadTask>(engine_, file));
}

void SyncTask::download(const std::shared_ptr<File>& file, const bool freeAfterDownload) {
    push(std::make_shared<DownloadTask>(engine_, file, freeAfterDownload));
}

void SyncTask::remove(const std::shared_ptr<File>& file, const DeleteTask::Type& type) {
    push(std::make_shared<DeleteTask>(engine_, file, type));
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

void SyncTask::interrupt() { interruptFlag_.store(true); }
bool SyncTask::isInterrupted() const { return interruptFlag_.load(); }
void SyncTask::handleInterrupt() const { if (isInterrupted()) throw std::runtime_error("Sync task interrupted"); }

