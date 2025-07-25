#pragma once

#include "concurrency/ThreadPool.hpp"

#include <atomic>
#include <memory>

using namespace vh::concurrency;

namespace vh::services {

class ThreadPoolRegistry {
public:
    static ThreadPoolRegistry& instance() {
        static ThreadPoolRegistry instance;
        return instance;
    }

    void init() {
        if (http_) return; // already initialized

        http_ = std::make_shared<ThreadPool>();
        stopFlag_.store(false);
    }

    void shutdown() {
        if (!stopFlag_.load()) {
            http_->stop();
            stopFlag_.store(true);
        }
    }

    std::shared_ptr<ThreadPool>& httpPool() { return http_; }

private:
    std::shared_ptr<ThreadPool> http_;
    std::atomic<bool> stopFlag_{false};
};

}
