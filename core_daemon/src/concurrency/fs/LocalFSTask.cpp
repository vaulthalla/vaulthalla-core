#include "concurrency/fs/LocalFSTask.hpp"
#include "concurrency/fs/LocalDeleteTask.hpp"
#include "database/Queries/FileQueries.hpp"
#include "database/Queries/OperationQueries.hpp"
#include "storage/LocalDiskStorageEngine.hpp"
#include "types/Sync.hpp"
#include "types/Vault.hpp"
#include "types/Operation.hpp"

#include <iostream>

using namespace vh::concurrency;
using namespace vh::database;
using namespace vh::storage;
using namespace vh::types;
using namespace std::chrono;

void LocalFSTask::operator()() {
    std::cout << "[LocalFSTask] Started sync for vault: " << engine_->getVault()->name << std::endl;
    const auto start = steady_clock::now();

    try {
        handleInterrupt();

        if (!engine_) {
            std::cerr << "[LocalFSTask] Engine not initialized!" << std::endl;
            return;
        }

        isRunning_ = true;

        removeTrashedFiles();
        handleInterrupt();

        processFutures();
    } catch (const std::exception& e) {
        std::cerr << "[LocalFSTask] Exception: " << e.what() << std::endl;
    }

    isRunning_ = false;
    auto end = steady_clock::now();
    std::cout << "[LocalFSTask] Finished sync in " << duration_cast<seconds>(end - start).count() << " seconds." << std::endl;
}

void LocalFSTask::removeTrashedFiles() {
    const auto files = FileQueries::listTrashedFiles(engine_->vaultId());
    futures_.reserve(files.size());
    for (const auto& file : files) push(std::make_shared<LocalDeleteTask>(localEngine(), file));
    processFutures();
}

void LocalFSTask::processOperations() const {
    const auto operations = OperationQueries::listOperationsByVault(engine_->vaultId());
    for (const auto& op : operations) {
        if (op->operation == Operation::Op::Copy) fs::copy(op->source_path, op->destination_path);
        else if (op->operation == Operation::Op::Move) fs::rename(op->source_path, op->destination_path);
        else if (op->operation == Operation::Op::Rename) fs::rename(op->source_path, op->destination_path);
        else throw std::runtime_error("Unknown operation type: " + std::to_string(static_cast<int>(op->operation)));
    }
}

std::shared_ptr<LocalDiskStorageEngine> LocalFSTask::localEngine() const {
    return std::static_pointer_cast<LocalDiskStorageEngine>(engine_);
}
