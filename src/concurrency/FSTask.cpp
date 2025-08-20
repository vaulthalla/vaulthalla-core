#include "concurrency/FSTask.hpp"
#include "concurrency/ThreadPoolManager.hpp"
#include "concurrency/ThreadPool.hpp"
#include "services/SyncController.hpp"
#include "storage/StorageEngine.hpp"
#include "types/Sync.hpp"
#include "types/Operation.hpp"
#include "types/Vault.hpp"
#include "types/Path.hpp"
#include "util/files.hpp"
#include "database/Queries/OperationQueries.hpp"
#include "database/Queries/FileQueries.hpp"
#include "crypto/VaultEncryptionManager.hpp"
#include "storage/Filesystem.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "services/LogRegistry.hpp"

using namespace vh::concurrency;
using namespace vh::storage;
using namespace vh::database;
using namespace vh::services;
using namespace vh::logging;
using namespace std::chrono;

FSTask::FSTask(const std::shared_ptr<StorageEngine>& engine)
: next_run(system_clock::from_time_t(engine->sync->last_sync_at)
           + seconds(engine->sync->interval.count())),
    engine_(engine) {}

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
            LogRegistry::sync()->error("[FSTask] Future failed");
    futures_.clear();
}

unsigned int FSTask::vaultId() const { return engine_->vault->id; }

void FSTask::requeue() {
    next_run = system_clock::now() + seconds(engine_->sync->interval.count());
    ServiceDepsRegistry::instance().syncController->requeue(shared_from_this());
}

void FSTask::push(const std::shared_ptr<Task>& task) {
    futures_.push_back(task->getFuture().value());
    ThreadPoolManager::instance().syncPool()->submit(task);
}

void FSTask::processOperations() const {
    for (const auto& op : OperationQueries::listOperationsByVault(engine_->vault->id)) {
        const auto absSrc = engine_->paths->absPath(op->source_path, PathType::BACKING_VAULT_ROOT);
        const auto absDest = engine_->paths->absPath(op->destination_path, PathType::BACKING_VAULT_ROOT);
        if (absDest.has_parent_path()) Filesystem::mkdir(absDest.parent_path());

        const auto f = FileQueries::getFileByPath(engine_->vault->id, op->destination_path);
        if (!f) {
            LogRegistry::sync()->error("[FSTask] File not found for operation: {}", op->destination_path);
            continue;
        }

        {
            const auto tmpPath = util::decrypt_file_to_temp(vaultId(), op->source_path, engine());
            const auto buffer = util::readFileToVector(tmpPath);

            const auto [ciphertext, version] = engine_->encryptionManager->encrypt(buffer, f->encryption_iv);
            f->encrypted_with_key_version = version;

            util::writeFile(absDest, ciphertext);
            FileQueries::setEncryptionIVAndVersion(f);
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

