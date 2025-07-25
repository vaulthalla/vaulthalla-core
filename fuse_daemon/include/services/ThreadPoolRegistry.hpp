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
        if (sync_ && fuse_) return; // already initialized

        sync_ = std::make_shared<ThreadPool>();
        fuse_ = std::make_shared<ThreadPool>();
        stopFlag_.store(false);
    }

    void shutdown() {
        if (!stopFlag_.load()) {
            sync_->stop();
            fuse_->stop();
            stopFlag_.store(true);
        }
    }

    std::shared_ptr<ThreadPool>& syncPool() { return sync_; }
    std::shared_ptr<ThreadPool>& fusePool() { return fuse_; }

private:
    std::shared_ptr<ThreadPool> sync_, fuse_;
    std::atomic<bool> stopFlag_{false};
};

}
