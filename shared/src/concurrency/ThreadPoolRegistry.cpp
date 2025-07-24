#include "concurrency/ThreadPoolRegistry.hpp"
#include "concurrency/ThreadPool.hpp"

using namespace vh::concurrency;

ThreadPoolRegistry& ThreadPoolRegistry::instance() {
    static ThreadPoolRegistry instance;
    return instance;
}

void ThreadPoolRegistry::init() {
    if (sync_ && thumb_ && http_) return; // already initialized

    sync_  = std::make_shared<ThreadPool>();
    thumb_ = std::make_shared<ThreadPool>();
    http_ = std::make_shared<ThreadPool>();
    stopFlag_.store(false);
}

void ThreadPoolRegistry::shutdown() {
    if (!stopFlag_.load()) {
        sync_->stop();
        thumb_->stop();
        http_->stop();
        stopFlag_.store(true);
    }
}

std::shared_ptr<ThreadPool>& ThreadPoolRegistry::syncPool() { return sync_; }
std::shared_ptr<ThreadPool>& ThreadPoolRegistry::thumbPool() { return thumb_; }
std::shared_ptr<ThreadPool>& ThreadPoolRegistry::httpPool() { return http_; }
