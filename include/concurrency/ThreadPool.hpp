#pragma once
#include "ThreadWorker.hpp"
#include "Task.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace vh::concurrency {

class ThreadPool {
public:
    explicit ThreadPool(std::string name, const size_t initialWorkers = 1)
        : name_(std::move(name)), stopFlag_(false) {
        addWorkers(initialWorkers);
    }

    ~ThreadPool() {
        stop();
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void stop() {
        stopFlag_.store(true, std::memory_order_relaxed);
        {
            std::scoped_lock lock(queueMutex_);
            cv_.notify_all();
        }
        for (auto& w : workers_) w->stop();
        workers_.clear();
    }

    void submit(std::shared_ptr<Task> task) {
        {
            std::scoped_lock lock(queueMutex_);
            taskQueue_.push_back(std::move(task));
        }
        cv_.notify_one();
    }

    void adoptWorker(std::unique_ptr<ThreadWorker> w) {
        std::scoped_lock lock(workerMutex_);
        workers_.push_back(std::move(w));
    }

    std::unique_ptr<ThreadWorker> donateWorker() {
        std::scoped_lock lock(workerMutex_);
        for (auto it = workers_.begin(); it != workers_.end(); ++it) {
            if ((*it)->isIdle()) {
                auto w = std::move(*it);
                workers_.erase(it);
                return w;
            }
        }
        return nullptr;
    }

    size_t pendingTasks() const {
        std::scoped_lock lock(queueMutex_);
        return taskQueue_.size();
    }

    size_t numWorkers() const {
        std::scoped_lock lock(workerMutex_);
        return workers_.size();
    }

    const std::string& name() const noexcept { return name_; }

    void addWorkers(size_t count) {
        std::scoped_lock lock(workerMutex_);
        for (size_t i = 0; i < count; ++i) workers_.push_back(std::move(std::make_unique<ThreadWorker>(this)));
        if (!feederThread_.joinable()) feederThread_ = std::thread([this] { feederLoop(); });
    }

private:
    void feederLoop() {
        while (!stopFlag_.load(std::memory_order_relaxed)) {
            std::shared_ptr<Task> task;
            {
                std::unique_lock lock(queueMutex_);
                cv_.wait(lock, [this] {
                    return stopFlag_.load() || !taskQueue_.empty();
                });
                if (stopFlag_.load() && taskQueue_.empty())
                    return;
                task = std::move(taskQueue_.front());
                taskQueue_.pop_front();
            }

            // Assign to first idle worker, else round‑robin
            ThreadWorker* target = nullptr;
            {
                std::scoped_lock lock(workerMutex_);
                for (auto& w : workers_) {
                    if (w->isIdle()) {
                        target = w.get();
                        break;
                    }
                }
                if (!target && !workers_.empty()) {
                    // round robin fallback
                    target = workers_[rrIndex_ % workers_.size()].get();
                    rrIndex_++;
                }
            }

            if (target) {
                target->enqueue(std::move(task));
            } else {
                // no worker? requeue
                std::scoped_lock lock(queueMutex_);
                taskQueue_.push_front(std::move(task));
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }

    std::string name_;
    std::atomic<bool> stopFlag_;
    std::deque<std::shared_ptr<Task>> taskQueue_;
    mutable std::mutex queueMutex_;
    std::condition_variable cv_;

    std::vector<std::unique_ptr<ThreadWorker>> workers_;
    mutable std::mutex workerMutex_;

    std::thread feederThread_;
    size_t rrIndex_{0};
};

}
