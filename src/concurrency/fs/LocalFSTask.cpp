#include "concurrency/fs/LocalFSTask.hpp"
#include "concurrency/fs/LocalDeleteTask.hpp"
#include "database/Queries/FileQueries.hpp"
#include "storage/StorageEngine.hpp"
#include "types/vault/Vault.hpp"
#include "logging/LogRegistry.hpp"
#include "concurrency/fs/LocalRotateKeyTask.hpp"
#include "types/sync/Event.hpp"
#include "types/sync/Throughput.hpp"

using namespace vh::concurrency;
using namespace vh::database;
using namespace vh::storage;
using namespace vh::types;
using namespace vh::logging;
using namespace std::chrono;

void LocalFSTask::operator()() {
    LogRegistry::sync()->debug("[LocalFSTask] Starting sync for vault '{}'", engine_->vault->id);
    event_->start();

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
        event_->stop();
        requeue();
        return;
    } catch (...) {
        LogRegistry::sync()->error("[LocalFSTask] Unknown exception during sync.");
        isRunning_ = false;
        event_->stop();
        requeue();
        return;
    }

    isRunning_ = false;
    event_->stop();
    LogRegistry::sync()->debug("[LocalFSTask] Sync completed for vault '{}' in {}s", engine_->vault->id, event_->durationSeconds());
    requeue();
}

void LocalFSTask::pushKeyRotationTask(const std::vector<std::shared_ptr<File> >& files, unsigned int begin, unsigned int end) {
    push(std::make_shared<LocalRotateKeyTask>(localEngine(), files, begin, end));
}

void LocalFSTask::removeTrashedFiles() {
    const auto engine = localEngine();
    const auto files = FileQueries::listTrashedFiles(engine_->vault->id);
    futures_.reserve(files.size());
    for (const auto& file : files) {
        auto& op = event_->getOrCreateThroughput(sync::Throughput::Metric::DELETE).newOp();
        push(std::make_shared<LocalDeleteTask>(engine, file, op));
    }
    processFutures();
}

std::shared_ptr<StorageEngine> LocalFSTask::localEngine() const {
    return std::static_pointer_cast<StorageEngine>(engine_);
}
