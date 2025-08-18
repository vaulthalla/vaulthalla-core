#include "services/AsyncService.hpp"
#include "services/ServiceManager.hpp"
#include "services/LogRegistry.hpp"

using namespace vh::services;
using namespace vh::logging;

AsyncService::AsyncService(const std::string& serviceName) : serviceName_(serviceName) {}

AsyncService::~AsyncService() {
    stop(); // ensure cleanup
}

void AsyncService::start() {
    if (isRunning()) return;

    interruptFlag_.store(false);
    running_.store(true);

    worker_ = std::thread([this] {
        try {
            runLoop();
        } catch (const std::exception& e) {
            LogRegistry::vaulthalla()->error("[{}] Service encountered an error: {}", serviceName_, e.what());
            running_.store(false);
            return;
        } catch (...) {
            LogRegistry::vaulthalla()->error("[{}] Service encountered an unknown error.", serviceName_);
            running_.store(false);
            return;
        }
        running_.store(false);
    });

    LogRegistry::vaulthalla()->info("[{}] Service started.", serviceName_);
}

void AsyncService::stop() {
    if (!isRunning()) return;

    LogRegistry::vaulthalla()->info("[{}] Stopping service...", serviceName_);
    interruptFlag_.store(true);

    // Only join if we’re not calling stop() from the same thread
    if (worker_.joinable() && std::this_thread::get_id() != worker_.get_id()) {
        worker_.join();
    }

    running_.store(false);
    interruptFlag_.store(false);

    LogRegistry::vaulthalla()->info("[{}] Service stopped.", serviceName_);
}

void AsyncService::restart() {
    LogRegistry::vaulthalla()->info("[{}] Restarting service...", serviceName_);
    stop();
    start();
}

void AsyncService::handleInterrupt() {
    if (interruptFlag_.load()) {
        LogRegistry::vaulthalla()->info("[{}] Service interrupted.", serviceName_);
        stop();
    }
}
