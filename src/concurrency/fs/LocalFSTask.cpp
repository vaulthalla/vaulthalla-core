#include "concurrency/fs/LocalFSTask.hpp"
#include "concurrency/fs/LocalDeleteTask.hpp"
#include "database/Queries/FileQueries.hpp"
#include "storage/StorageEngine.hpp"
#include "types/Sync.hpp"
#include "types/Vault.hpp"
#include "util/files.hpp"

#include <iostream>

using namespace vh::concurrency;
using namespace vh::database;
using namespace vh::storage;
using namespace vh::types;
using namespace std::chrono;

void LocalFSTask::operator()() {
    std::cout << "[LocalFSTask] Started sync for vault: " << engine_->vault->name << std::endl;
    const auto start = steady_clock::now();

    try {
        handleInterrupt();

        if (!engine_) {
            std::cerr << "[LocalFSTask] Engine not initialized!" << std::endl;
            return;
        }

        isRunning_ = true;

        processOperations();
        handleInterrupt();
        removeTrashedFiles();
        handleInterrupt();
    } catch (const std::exception& e) {
        std::cerr << "[LocalFSTask] Exception: " << e.what() << std::endl;
    }

    isRunning_ = false;
    const auto end = steady_clock::now();
    std::cout << "[LocalFSTask] Finished sync in " << duration_cast<seconds>(end - start).count() << " seconds." << std::endl;
    requeue();
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
