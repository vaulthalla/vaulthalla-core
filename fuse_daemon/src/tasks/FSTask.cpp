#include "tasks/FSTask.hpp"
#include "services/ThreadPoolRegistry.hpp"
#include "concurrency/ThreadPool.hpp"
#include "services/SyncController.hpp"
#include "storage/StorageEngine.hpp"
#include "types/Sync.hpp"
#include "types/Operation.hpp"
#include "types/Vault.hpp"
#include "util/files.hpp"
#include "database/Queries/OperationQueries.hpp"
#include "database/Queries/FileQueries.hpp"
#include "engine/VaultEncryptionManager.hpp"
#include "storage/Filesystem.hpp"

#include <iostream>

using namespace vh::concurrency;
using namespace vh::storage;
using namespace vh::database;
using namespace vh::services;
using namespace std::chrono;

FSTask::FSTask(const std::shared_ptr<StorageEngine>& engine, const std::shared_ptr<SyncController>& controller)
: next_run(system_clock::from_time_t(engine->sync->last_sync_at)
           + seconds(engine->sync->interval.count())),
    engine_(engine), controller_(controller) {
    if (!controller_) throw std::invalid_argument("SyncController cannot be null");
}

void FSTask::handleInterrupt() const { if (isInterrupted()) throw std::runtime_error("Sync task interrupted"); }

bool FSTask::isRunning() const { return isRunning_; }

void FSTask::interrupt() { interruptFlag_.store(true); }

bool FSTask::isInterrupted() const { return interruptFlag_.load(); }

std::shared_ptr<StorageEngine> FSTask::engine() const {
    if (!engine_) throw std::runtime_error("Storage engine is not set");
    return engine_;
}

void FSTask::processFutures() {
    for (auto& f : futures_)
        if (std::get<bool>(f.get()) == false)
            std::cerr << "[FSTask] A filesystem task failed." << std::endl;
    futures_.clear();
}

unsigned int FSTask::vaultId() const { return engine_->vault->id; }

void FSTask::requeue() {
    next_run = system_clock::now() + seconds(engine_->sync->interval.count());
    if (controller_) controller_->requeue(shared_from_this());
    else throw std::runtime_error("SyncController is not set, cannot requeue task");
}

void FSTask::push(const std::shared_ptr<Task>& task) {
    futures_.push_back(task->getFuture().value());
    ThreadPoolRegistry::instance().syncPool()->submit(task);
}

void FSTask::processOperations() const {
    for (const auto& op : OperationQueries::listOperationsByVault(engine_->vault->id)) {
        const auto absSrc = engine_->getAbsolutePath(op->source_path);
        const auto absDest = engine_->getAbsolutePath(op->destination_path);
        if (absDest.has_parent_path()) Filesystem::mkdir(absDest.parent_path());

        {
            const auto tmpPath = util::decrypt_file_to_temp(vaultId(), op->source_path, engine());
            const auto buffer = util::readFileToVector(tmpPath);

            std::string iv_b64;
            const auto ciphertext = engine_->encryptionManager->encrypt(buffer, iv_b64);

            util::writeFile(absDest, ciphertext);
            FileQueries::setEncryptionIV(vaultId(), op->destination_path, iv_b64);
        }

        const auto& move = [&]() {
            if (fs::exists(absSrc)) fs::remove(absSrc);
            engine_->moveThumbnails(op->source_path, op->destination_path);
        };

        if (op->operation == Operation::Op::Copy) engine_->copyThumbnails(op->source_path, op->destination_path);
        else if (op->operation == Operation::Op::Move || op->operation == Operation::Op::Rename) move();
        else throw std::runtime_error("Unknown operation type: " + std::to_string(static_cast<int>(op->operation)));
    }
}

