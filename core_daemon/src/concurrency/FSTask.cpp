#include "concurrency/FSTask.hpp"
#include "concurrency/ThreadPoolRegistry.hpp"
#include "concurrency/ThreadPool.hpp"
#include "services/SyncController.hpp"
#include "storage/StorageEngine.hpp"
#include "types/Sync.hpp"
#include "types/Vault.hpp"

#include <iostream>

using namespace vh::concurrency;
using namespace std::chrono;

FSTask::FSTask(const std::shared_ptr<storage::StorageEngine>& engine, const std::shared_ptr<services::SyncController>& controller)
: next_run(system_clock::from_time_t(engine->sync_->last_sync_at)
           + seconds(engine->sync_->interval.count())),
    engine_(engine), controller_(controller) {
    if (!controller_) throw std::invalid_argument("SyncController cannot be null");
}

void FSTask::handleInterrupt() const { if (isInterrupted()) throw std::runtime_error("Sync task interrupted"); }

bool FSTask::isRunning() const { return isRunning_; }

void FSTask::interrupt() { interruptFlag_.store(true); }

bool FSTask::isInterrupted() const { return interruptFlag_.load(); }

std::shared_ptr<vh::storage::StorageEngine> FSTask::engine() const {
    if (!engine_) throw std::runtime_error("Storage engine is not set");
    return engine_;
}

void FSTask::processFutures() {
    for (auto& f : futures_)
        if (std::get<bool>(f.get()) == false)
            std::cerr << "[FSTask] A filesystem task failed." << std::endl;
    futures_.clear();
}

unsigned int FSTask::vaultId() const { return engine_->vaultId(); }

void FSTask::requeue() {
    next_run = system_clock::now() + seconds(engine_->sync_->interval.count());
    if (controller_) controller_->requeue(shared_from_this());
    else throw std::runtime_error("SyncController is not set, cannot requeue task");
}

void FSTask::push(const std::shared_ptr<Task>& task) {
    futures_.push_back(task->getFuture().value());
    ThreadPoolRegistry::instance().syncPool()->submit(task);
}
