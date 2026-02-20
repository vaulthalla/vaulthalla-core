#include "services/AsyncService.hpp"
#include "logging/LogRegistry.hpp"

using namespace vh::services;
using namespace vh::logging;

AsyncService::AsyncService(const std::string& serviceName)
    : serviceName_(serviceName) {}

AsyncService::~AsyncService() {
    stop();
}

void AsyncService::start() {
    if (isRunning()) return;

    interruptFlag_.store(false, std::memory_order_release);
    running_.store(true, std::memory_order_release);

    worker_ = std::thread([this] {
        try {
            runLoop();
        } catch (const std::exception& e) {
            LogRegistry::vaulthalla()->error("[{}] Service error: {}", serviceName_, e.what());
        } catch (...) {
            LogRegistry::vaulthalla()->error("[{}] Service error: unknown exception.", serviceName_);
        }

        running_.store(false, std::memory_order_release);
    });

    LogRegistry::vaulthalla()->info("[{}] Service started.", serviceName_);
}

void AsyncService::stop() {
    if (!isRunning()) return;

    LogRegistry::vaulthalla()->info("[{}] Stopping service...", serviceName_);
    interruptFlag_.store(true, std::memory_order_release);

    if (worker_.joinable() && std::this_thread::get_id() != worker_.get_id()) {
        worker_.join();
    }

    running_.store(false, std::memory_order_release);
    // Leave interruptFlag_ true until next start() resets it
    LogRegistry::vaulthalla()->info("[{}] Service stopped.", serviceName_);
}

void AsyncService::restart() {
    LogRegistry::vaulthalla()->info("[{}] Restarting service...", serviceName_);
    stop();
    start();
}
