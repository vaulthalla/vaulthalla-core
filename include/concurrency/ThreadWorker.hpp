#pragma once
#include "Task.hpp"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace vh::concurrency {

class ThreadPool; // forward decl

class ThreadWorker {
public:
    explicit ThreadWorker(ThreadPool* owner)
        : owner_(owner), stopFlag_(false), busy_(false) {
        thread_ = std::thread([this] { run(); });
    }

    ~ThreadWorker() {
        stop();
        if (thread_.joinable()) thread_.join();
    }

    ThreadWorker(const ThreadWorker&) = delete;
    ThreadWorker& operator=(const ThreadWorker&) = delete;

    ThreadWorker(ThreadWorker&& other) noexcept {
        moveFrom(std::move(other));
    }

    ThreadWorker& operator=(ThreadWorker&& other) noexcept {
        if (this != &other) {
            stop();
            if (thread_.joinable()) thread_.join();
            moveFrom(std::move(other));
        }
        return *this;
    }

    void stop() {
        stopFlag_.store(true, std::memory_order_relaxed);
        cv_.notify_all();
    }

    [[nodiscard]] bool isBusy() const noexcept { return busy_.load(std::memory_order_relaxed); }
    [[nodiscard]] bool isIdle() const noexcept { return !isBusy(); }

    void enqueue(std::shared_ptr<Task> task) {
        {
            std::unique_lock lock(mutex_);
            queue_.push(std::move(task));
        }
        cv_.notify_one();
    }

    void rebind(ThreadPool* newOwner) {
        std::scoped_lock lock(ownerMutex_);
        owner_ = newOwner;
        cv_.notify_one(); // poke in case it was idling
    }

private:
    void run() {
        while (!stopFlag_.load(std::memory_order_relaxed)) {
            std::shared_ptr<Task> task;

            {
                std::unique_lock lock(mutex_);
                cv_.wait(lock, [this] {
                    return stopFlag_.load() || !queue_.empty();
                });
                if (stopFlag_.load() && queue_.empty())
                    return;
                task = std::move(queue_.front());
                queue_.pop();
                busy_.store(true, std::memory_order_relaxed);
            }

            if (task) (*task)();

            busy_.store(false, std::memory_order_relaxed);
        }
    }

    void moveFrom(ThreadWorker&& other) {
        owner_ = other.owner_;
        stopFlag_.store(other.stopFlag_.load());
        busy_.store(other.busy_.load());
        thread_ = std::move(other.thread_);
        // don’t steal queue or mutex — let other drain on destruction
    }

    ThreadPool* owner_{nullptr};
    std::atomic<bool> stopFlag_;
    std::atomic<bool> busy_;
    std::queue<std::shared_ptr<Task>> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::mutex ownerMutex_;
    std::thread thread_;
};

}
