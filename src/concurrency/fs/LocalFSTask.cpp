#include "concurrency/fs/LocalFSTask.hpp"
#include "concurrency/fs/LocalDeleteTask.hpp"
#include "database/Queries/FileQueries.hpp"
#include "storage/StorageEngine.hpp"
#include "types/vault/Vault.hpp"
#include "logging/LogRegistry.hpp"
#include "concurrency/fs/LocalRotateKeyTask.hpp"

using namespace vh::concurrency;
using namespace vh::database;
using namespace vh::storage;
using namespace vh::types;
using namespace vh::logging;
using namespace std::chrono;

void LocalFSTask::operator()() {
    LogRegistry::sync()->debug("[LocalFSTask] Starting sync for vault '{}'", engine_->vault->id);
    const auto start = steady_clock::now();

    try {
        handleInterrupt();

        if (!engine_) {
            LogRegistry::sync()->error("[LocalFSTask] Engine is null, cannot proceed with sync.");
            return;
        }

        isRunning_ = true;

        processOperations();
        handleInterrupt();
        removeTrashedFiles();
        handleInterrupt();
        handleVaultKeyRotation();
        handleInterrupt();
    } catch (const std::exception& e) {
        LogRegistry::sync()->error("[LocalFSTask] Exception during sync: {}", e.what());
        isRunning_ = false;
        requeue();
        return;
    } catch (...) {
        LogRegistry::sync()->error("[LocalFSTask] Unknown exception during sync.");
        isRunning_ = false;
        requeue();
        return;
    }

    isRunning_ = false;
    const auto end = steady_clock::now();
    const auto duration = duration_cast<milliseconds>(end - start);
    LogRegistry::sync()->debug("[LocalFSTask] Sync completed for vault '{}' in {}ms", engine_->vault->id, duration.count());
    requeue();
}

void LocalFSTask::pushKeyRotationTask(const std::vector<std::shared_ptr<File> >& files, unsigned int begin, unsigned int end) {
    push(std::make_shared<LocalRotateKeyTask>(localEngine(), files, begin, end));
}

void LocalFSTask::removeTrashedFiles() {
    const auto engine = localEngine();
    const auto files = FileQueries::listTrashedFiles(engine_->vault->id);
    futures_.reserve(files.size());
    for (const auto& file : files) push(std::make_shared<LocalDeleteTask>(engine, file));
    processFutures();
}

std::shared_ptr<StorageEngine> LocalFSTask::localEngine() const {
    return std::static_pointer_cast<StorageEngine>(engine_);
}
