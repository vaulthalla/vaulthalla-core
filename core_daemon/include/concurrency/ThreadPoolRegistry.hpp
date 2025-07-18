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
    std::shared_ptr<ThreadPool>& thumbPool();
    std::shared_ptr<ThreadPool>& httpPool();

private:
    std::shared_ptr<ThreadPool> sync_;
    std::shared_ptr<ThreadPool> thumb_;
    std::shared_ptr<ThreadPool> http_;
    std::atomic<bool> stopFlag_{false};
};


}
