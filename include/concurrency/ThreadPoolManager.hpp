#pragma once
#include "ThreadPool.hpp"

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace vh::concurrency {

class ThreadPoolManager {
public:
    static ThreadPoolManager& instance() {
        static ThreadPoolManager inst;
        return inst;
    }

    void init(size_t reserveFactor = 3) {
        if (running_.load()) return;

        totalThreads_ = std::max(std::thread::hardware_concurrency() * reserveFactor, static_cast<size_t>(12));
        for (size_t i = 0; i < totalThreads_; ++i) reserve_.push_back(std::make_unique<ThreadWorker>(nullptr));

        pools_["fuse"]  = std::make_shared<ThreadPool>("fuse", 4);
        pools_["http"]  = std::make_shared<ThreadPool>("http", 3);
        pools_["thumb"] = std::make_shared<ThreadPool>("thumb", 2);
        pools_["sync"]  = std::make_shared<ThreadPool>("sync", 3);

        highPressureFactor_ = totalThreads_ / pools_.size();

        stopFlag_ = false;
        monitor_ = std::thread([this] { rebalanceLoop(); });
        running_.store(true);
    }

    void shutdown() {
        stopFlag_ = true;
        if (monitor_.joinable()) monitor_.join();

        for (auto& [_, pool] : pools_) {
            pool->stop();
        }
        pools_.clear();
        reserve_.clear();
        running_.store(false);
    }

    std::shared_ptr<ThreadPool> get(const std::string& name) {
        auto it = pools_.find(name);
        if (it == pools_.end()) return nullptr;
        return it->second;
    }

    std::shared_ptr<ThreadPool>& fusePool() { return pools_["fuse"]; }
    std::shared_ptr<ThreadPool>& httpPool() { return pools_["http"]; }
    std::shared_ptr<ThreadPool>& thumbPool() { return pools_["thumb"]; }
    std::shared_ptr<ThreadPool>& syncPool() { return pools_["sync"]; }

private:
    ThreadPoolManager() = default;
    ~ThreadPoolManager() { shutdown(); }

    void rebalanceLoop() {
        using namespace std::chrono_literals;
        while (!stopFlag_.load()) {
            for (auto& [name, pool] : pools_) {
                const auto pending = pool->pendingTasks();
                const auto workers = pool->numWorkers();

                // high pressure → try reserve, then steal
                if (pending > workers * highPressureFactor_) {
                    if (reserve_.empty()) stealFor(pool);
                    else {
                        pool->adoptWorker(std::move(reserve_.back()));
                        reserve_.pop_back();
                    }
                }

                // low pressure → consider returning a worker to reserve
                if (pending < workers * lowPressureFactor_ && workers > minPoolSize(name)) {
                    auto w = pool->donateWorker();
                    if (w) reserve_.push_back(std::move(w));
                }
            }
            std::this_thread::sleep_for(50ms);
        }
    }

    void stealFor(const std::shared_ptr<ThreadPool>& needy) {
        for (auto& [donorName, donor] : pools_) {
            if (donor == needy) continue;
            if (priorityOf(donorName) < priorityOf(needy->name())) continue;
            if (donor->pendingTasks() < donor->numWorkers() / 2) {
                if (auto w = donor->donateWorker()) {
                    needy->adoptWorker(std::move(w));
                    return;
                }
            }
        }
    }

    [[nodiscard]] static int priorityOf(const std::string& name) {
        if (name == "fuse") return 3;
        if (name == "http") return 2;
        if (name == "thumb") return 1;
        if (name == "sync") return 0;
        return 0;
    }

    [[nodiscard]] static size_t minPoolSize(const std::string& name) {
        if (name == "fuse") return 2;
        if (name == "http") return 2;
        if (name == "thumb") return 1;
        if (name == "sync") return 1;
        return 1;
    }

    std::map<std::string, std::shared_ptr<ThreadPool>> pools_;
    std::vector<std::unique_ptr<ThreadWorker>> reserve_;
    std::thread monitor_;
    std::atomic<bool> stopFlag_{false};
    std::atomic<bool> running_{false};
    size_t totalThreads_{0};
    size_t highPressureFactor_ = 4; // tasks per worker before scaling up
    const size_t lowPressureFactor_  = 1; // scale down if <1× tasks per worker
};

}
