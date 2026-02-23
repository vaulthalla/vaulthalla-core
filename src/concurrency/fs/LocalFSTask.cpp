#include "concurrency/fs/LocalFSTask.hpp"
#include "concurrency/fs/LocalDeleteTask.hpp"
#include "database/Queries/FileQueries.hpp"
#include "storage/StorageEngine.hpp"
#include "types/vault/Vault.hpp"
#include "logging/LogRegistry.hpp"
#include "types/sync/Throughput.hpp"

using namespace vh::concurrency;
using namespace vh::database;
using namespace vh::storage;
using namespace vh::types;
using namespace vh::logging;
using namespace std::chrono;

void LocalFSTask::operator()() {
    startTask();

    const Stage stages[] = {
        {"shared",   [this]{ processSharedOps(); }}
    };

    runStages(stages);
    shutdown();
}

void LocalFSTask::removeTrashedFiles() {
    const auto engine = localEngine();
    const auto files = FileQueries::listTrashedFiles(engine->vault->id);

    futures.reserve(files.size());
    for (const auto& file : files)
        push(std::make_shared<LocalDeleteTask>(engine, file, op(sync::Throughput::Metric::DELETE)));

    processFutures();
}

std::shared_ptr<StorageEngine> LocalFSTask::localEngine() const {
    return std::static_pointer_cast<StorageEngine>(engine);
}
