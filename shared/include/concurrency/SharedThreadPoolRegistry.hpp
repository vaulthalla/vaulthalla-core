#pragma once

#include "concurrency/ThreadPool.hpp"

#include <atomic>
#include <memory>

namespace vh::concurrency {

class SharedThreadPoolRegistry {
public:
    static SharedThreadPoolRegistry& instance() {
        static SharedThreadPoolRegistry instance;
        return instance;
    }

    void init() {
        if (thumb_) return; // already initialized

        thumb_ = std::make_shared<ThreadPool>();
        stopFlag_.store(false);
    }

    void shutdown() {
        if (!stopFlag_.load()) {
            thumb_->stop();
            stopFlag_.store(true);
        }
    }

    std::shared_ptr<ThreadPool>& thumbPool() { return thumb_; }

private:
    std::shared_ptr<ThreadPool> thumb_;
    std::atomic<bool> stopFlag_{false};
};

}
