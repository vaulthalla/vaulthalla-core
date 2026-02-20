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
    startTask();

    try {
        processSharedOps();
    } catch (const std::exception& e) {
        handleError(std::format("[LocalFSTask] Exception during sync: {}", e.what()));
    } catch (...) {
        handleError("[LocalFSTask] Unknown exception during sync.");
    }

    shutdown();
}

void LocalFSTask::pushKeyRotationTask(const std::vector<std::shared_ptr<File> >& files, unsigned int begin, unsigned int end) {
    push(std::make_shared<LocalRotateKeyTask>(localEngine(), files, begin, end));
}

void LocalFSTask::removeTrashedFiles() {
    const auto engine = localEngine();
    const auto files = FileQueries::listTrashedFiles(engine_->vault->id);

    futures_.reserve(files.size());
    for (const auto& file : files)
        push(std::make_shared<LocalDeleteTask>(engine, file, op(sync::Throughput::Metric::DELETE)));

    processFutures();
}

std::shared_ptr<StorageEngine> LocalFSTask::localEngine() const {
    return std::static_pointer_cast<StorageEngine>(engine_);
}
