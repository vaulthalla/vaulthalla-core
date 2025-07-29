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
        if (fuse_ && sync_ && thumb_ && http_) return; // already initialized

        fuse_ = std::make_shared<ThreadPool>();
        sync_ = std::make_shared<ThreadPool>();
        thumb_ = std::make_shared<ThreadPool>();
        http_ = std::make_shared<ThreadPool>();
        stopFlag_.store(false);
    }

    void shutdown() {
        if (!stopFlag_.load()) {
            fuse_->stop();
            sync_->stop();
            thumb_->stop();
            http_->stop();
            stopFlag_.store(true);
        }
    }

    std::shared_ptr<ThreadPool>& fusePool() { return fuse_; }
    std::shared_ptr<ThreadPool>& syncPool() { return sync_; }
    std::shared_ptr<ThreadPool>& thumbPool() { return thumb_; }
    std::shared_ptr<ThreadPool>& httpPool() { return http_; }

private:
    std::shared_ptr<ThreadPool> fuse_, sync_, thumb_, http_;
    std::atomic<bool> stopFlag_{false};
};

}
