#pragma once

#include <atomic>
#include <memory>

namespace vh::concurrency {

class ThreadPool;

class ThreadPoolRegistry {
public:
    static ThreadPoolRegistry& instance();

    void init();
    void shutdown();

    std::shared_ptr<ThreadPool>& syncPool();
    std::shared_ptr<ThreadPool>& cloudPool();
    std::shared_ptr<ThreadPool>& thumbPool();

private:
    std::shared_ptr<ThreadPool> sync_;
    std::shared_ptr<ThreadPool> cloud_;
    std::shared_ptr<ThreadPool> thumb_;
    std::atomic<bool> stopFlag_{false};
};


}
