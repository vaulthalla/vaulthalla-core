#include "concurrency/ThreadPool.hpp"

#include <utility>
#include <algorithm>
#include <iostream>

using namespace vh::concurrency;

ThreadPool::ThreadPool()
    : stopFlag(false),
      numThreads(std::thread::hardware_concurrency()),
      threads(numThreads),
      interruptFlag(std::make_shared<std::atomic<bool>>(false)) {
    for (auto& thread : threads)
        thread = std::thread(&ThreadPool::worker, this);
}

ThreadPool::ThreadPool(const std::shared_ptr<std::atomic<bool>>& interruptFlag)
    : stopFlag(false),
      numThreads(std::thread::hardware_concurrency()),
      threads(numThreads),
      interruptFlag(interruptFlag) {
    for (auto& thread : threads)
        thread = std::thread(&ThreadPool::worker, this);
}

ThreadPool::~ThreadPool() {
    stop();
    for (auto& thread : threads)
        if (thread.joinable()) thread.join();
}

ThreadPool::ThreadPool(const ThreadPool& rhs)
    : stopFlag(rhs.stopFlag.load()),
      numThreads(rhs.numThreads),
      threads(numThreads),
      interruptFlag(rhs.interruptFlag) {}

ThreadPool& ThreadPool::operator=(const ThreadPool& rhs) {
    if (this != &rhs) {
        stopFlag = rhs.stopFlag.load();
        interruptFlag = rhs.interruptFlag;
        numThreads = rhs.numThreads;
    }
    return *this;
}

void ThreadPool::stop() {
    stopFlag.store(true);
    cv.notify_all();
}

bool ThreadPool::interrupted() const {
    return interruptFlag->load();
}

void ThreadPool::interrupt() {
    interruptFlag->store(true);
    cv.notify_all();
}

void ThreadPool::worker() {
    while (!stopFlag.load()) {
        std::shared_ptr<Task> task;

        {
            std::unique_lock lock(mutex);
            cv.wait(lock, [this]() {
                return !queue.empty() || stopFlag.load();
            });

            if (stopFlag.load() && queue.empty()) return;

            task = std::move(queue.front());
            queue.pop();
        }

        try {
            if (task) (*task)(); // calls operator()()
        } catch (const std::exception& e) {
            std::cerr << "[ThreadPool] Task threw exception: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "[ThreadPool] Task threw unknown exception\n";
        }
    }
}
