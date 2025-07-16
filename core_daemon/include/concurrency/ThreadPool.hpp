#pragma once

#include "concurrency/Task.hpp"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <atomic>

namespace vh::concurrency {

class ThreadPool {
    std::atomic<bool> stopFlag;
    unsigned int numThreads;
    std::vector<std::thread> threads;
    std::condition_variable cv;
    std::queue<std::shared_ptr<Task>> queue;
    std::mutex mutex;

    void worker();

public:
    std::shared_ptr<std::atomic<bool>> interruptFlag;

    ThreadPool();
    ~ThreadPool();

    explicit ThreadPool(const std::shared_ptr<std::atomic<bool>>& interruptFlag);

    ThreadPool(const ThreadPool& rhs);
    ThreadPool& operator=(const ThreadPool& rhs);

    void stop();
    bool interrupted() const;
    void interrupt();

    void submit(std::shared_ptr<Task> task) {
        std::scoped_lock lock(mutex);
        queue.push(std::move(task));
        cv.notify_one();
    }
};

}
