#include "concurrency/FSTask.hpp"
#include "concurrency/ThreadPoolManager.hpp"
#include "concurrency/ThreadPool.hpp"
#include "services/SyncController.hpp"
#include "storage/StorageEngine.hpp"
#include "types/sync/Policy.hpp"
#include "types/sync/Operation.hpp"
#include "types/vault/Vault.hpp"
#include "types/fs/File.hpp"
#include "types/fs/Path.hpp"
#include "util/files.hpp"
#include "database/Queries/OperationQueries.hpp"
#include "database/Queries/FileQueries.hpp"
#include "crypto/VaultEncryptionManager.hpp"
#include "storage/Filesystem.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "logging/LogRegistry.hpp"
#include "util/task.hpp"
#include "types/sync/Event.hpp"
#include "types/sync/Throughput.hpp"
#include "types/sync/ScopedOp.hpp"
#include "database/Queries/SyncQueries.hpp"

using namespace vh::concurrency;
using namespace vh::storage;
using namespace vh::database;
using namespace vh::services;
using namespace vh::logging;
using namespace std::chrono;
using namespace vh::types;

FSTask::FSTask(const std::shared_ptr<StorageEngine>& engine)
: next_run(system_clock::from_time_t(engine->sync->last_sync_at) + seconds(engine->sync->interval.count())),
  engine_(engine),
  event_(std::make_shared<sync::Event>()) {}

void FSTask::handleInterrupt() const { if (isInterrupted()) throw std::runtime_error("Sync task interrupted"); }

bool FSTask::isRunning() const { return isRunning_; }

void FSTask::interrupt() { interruptFlag_.store(true); }

bool FSTask::isInterrupted() const { return interruptFlag_.load(); }

std::shared_ptr<StorageEngine> FSTask::engine() const {
    if (!engine_) throw std::runtime_error("Storage engine is not set");
    return engine_;
}

void FSTask::runStages(const std::span<const Stage> stages) const {
    for (const auto& [name, fn] : stages) {
        if (!isRunning()) break;
        try {
            fn();
            handleInterrupt();
            if (event_) event_->heartbeat();
        } catch (const std::exception& e) {
            handleError(std::format("[FSTask:{}] {}", std::string(name), e.what()));
            break;
        } catch (...) {
            handleError(std::format("[FSTask:{}] Unknown exception", std::string(name)));
            break;
        }
    }
}

void FSTask::startTask() {
    if (!engine_) {
        LogRegistry::sync()->error("[FSTask] Engine is null, cannot proceed with sync.");
        return;
    }

    LogRegistry::sync()->debug("[FSTask] Starting sync for vault '{}'", engine_->vault->id);

    isRunning_ = true;
    SyncQueries::reportSyncStarted(engine_->sync->id);

    newEvent();
    event_ = engine_->latestSyncEvent;
    event_->status = sync::Event::Status::RUNNING;
    engine_->saveSyncEvent();
    event_->start();
}

void FSTask::processSharedOps() {
    struct NamedOp { const char* name; std::function<void()> fn; };

    const std::vector<NamedOp> ops = {
        {"processOperations", [this]{ processOperations(); }},
        {"removeTrashedFiles", [this]{ removeTrashedFiles(); }},
        {"handleVaultKeyRotation", [this]{ handleVaultKeyRotation(); }},
      };

    for (const auto& [name, op] : ops) {
        if (!isRunning()) break;

        try {
            op();
            handleInterrupt();
            event_->heartbeat();
        } catch (const std::exception& e) {
            handleError(std::format("[FSTask] Exception during {}: {}", name, e.what()));
            break;
        }
    }
}

void FSTask::handleError(const std::string& message) const {
    LogRegistry::sync()->error("[FSTask] {}", message);
    event_->error_message = message;
    event_->status = sync::Event::Status::ERROR;
}

void FSTask::shutdown() {
    isRunning_ = false;
    futures_.clear();
    event_->stop();
    event_->parseCurrentStatus();
    engine_->saveSyncEvent();
    if (event_->status == sync::Event::Status::SUCCESS) {
        SyncQueries::reportSyncSuccess(engine_->sync->id);
        next_run = system_clock::now() + seconds(engine_->sync->interval.count());
        requeue();
        LogRegistry::sync()->debug("[FSTask] Sync task requeued for vault '{}'", engine_->vault->id);
        LogRegistry::sync()->info("[FSTask] Sync completed for vault '{}' in {}s",
                              engine_->vault->id, event_->durationSeconds());
    } else {
        LogRegistry::sync()->error("[FSTask] Sync failed for vault '{}': {}", engine_->vault->id, event_->error_message);
    }
}

void FSTask::newEvent() {
    if (!runNow_) engine_->newSyncEvent();
    else {
        engine_->newSyncEvent(trigger_);
        runNow_ = false;
    }
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

void FSTask::runNow(const uint8_t trigger) {
    runNow_ = true;
    trigger_ = trigger;
    next_run = system_clock::now();
}

void FSTask::push(const std::shared_ptr<Task>& task) {
    futures_.push_back(task->getFuture().value());
    ThreadPoolManager::instance().syncPool()->submit(task);
}

sync::ScopedOp& FSTask::op(const sync::Throughput::Metric& metric) const {
    return event_->getOrCreateThroughput(metric).newOp();
}

void FSTask::processOperations() const {
    for (const auto& op : OperationQueries::listOperationsByVault(engine_->vault->id)) {
        auto& scopedOp = event_->getOrCreateThroughput(op->opToThroughputMetric()).newOp();
        scopedOp.start();

        const auto absSrc = engine_->paths->absPath(op->source_path, PathType::BACKING_VAULT_ROOT);
        const auto absDest = engine_->paths->absPath(op->destination_path, PathType::BACKING_VAULT_ROOT);
        if (absDest.has_parent_path()) Filesystem::mkdir(absDest.parent_path());

        const auto f = FileQueries::getFileByPath(engine_->vault->id, op->destination_path);
        if (!f) {
            LogRegistry::sync()->error("[FSTask] File not found for operation: {}", op->destination_path);
            scopedOp.stop();
            continue;
        }

        scopedOp.size_bytes = f->size_bytes;

        if (f->size_bytes == 0 && op->operation != sync::Operation::Op::Copy) {
            LogRegistry::sync()->error("[FSTask] File size is zero for operation: {}", op->destination_path);
            scopedOp.stop();
            continue;
        }

        const auto tmpPath = util::decrypt_file_to_temp(vaultId(), op->source_path, engine());
        const auto buffer = util::readFileToVector(tmpPath);

        if (buffer.empty()) {
            LogRegistry::sync()->error("[FSTask] Empty file buffer for operation: {}", op->source_path);
            scopedOp.stop();
            continue;
        }

        const auto ciphertext = engine_->encryptionManager->encrypt(buffer, f);
        util::writeFile(absDest, ciphertext);
        FileQueries::setEncryptionIVAndVersion(f);

        const auto& move = [&]() {
            if (fs::exists(absSrc)) fs::remove(absSrc);
            engine_->moveThumbnails(op->source_path, op->destination_path);
        };

        if (op->operation == sync::Operation::Op::Copy) engine_->copyThumbnails(op->source_path, op->destination_path);
        else if (op->operation == sync::Operation::Op::Move || op->operation == sync::Operation::Op::Rename) move();
        else throw std::runtime_error("Unknown operation type: " + std::to_string(static_cast<int>(op->operation)));

        scopedOp.stop();
    }
}

void FSTask::handleVaultKeyRotation() {
    try {
        if (!engine_->encryptionManager->rotation_in_progress()) return;

        const auto filesToRotate = FileQueries::getFilesOlderThanKeyVersion(engine_->vault->id, engine_->encryptionManager->get_key_version());
        if (filesToRotate.empty()) {
            LogRegistry::audit()->info("[FSTask] No files to rotate for vault '{}'", engine_->vault->id);
            engine_->encryptionManager->finish_key_rotation();
            return;
        }

        for (const auto& [begin, end] : getTaskOperationRanges(filesToRotate.size()))
            pushKeyRotationTask(filesToRotate, begin, end);

        processFutures();

        engine_->encryptionManager->finish_key_rotation();

        LogRegistry::audit()->info("[FSTask] Vault key rotation finished for vault '{}'", engine_->vault->id);
    } catch (const std::exception& e) {
        LogRegistry::sync()->error("[FSTask] Exception during vault key rotation for vault '{}': {}", engine_->vault->id, e.what());
        isRunning_ = false;
    } catch (...) {
        LogRegistry::sync()->error("[FSTask] Unknown exception during vault key rotation for vault '{}'", engine_->vault->id);
        isRunning_ = false;
    }
}
