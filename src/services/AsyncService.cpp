#include "services/AsyncService.hpp"
#include "services/ServiceManager.hpp"

#include <iostream>

using namespace vh::services;

AsyncService::AsyncService(const std::shared_ptr<ServiceManager>& serviceManager,
                           const std::string& serviceName)
    : serviceManager_(serviceManager), serviceName_(serviceName) {
    if (!serviceManager_) throw std::runtime_error("ServiceManager cannot be null");
}

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
            std::cerr << "[" << serviceName_ << "] Exception: " << e.what() << std::endl;
        }
        running_.store(false);
    });

    std::cout << "[" << serviceName_ << "] Service started." << std::endl;
}

void AsyncService::stop() {
    if (!isRunning()) return;

    std::cout << "[" << serviceName_ << "] Stopping service..." << std::endl;
    interruptFlag_.store(true);

    // Only join if we’re not calling stop() from the same thread
    if (worker_.joinable() && std::this_thread::get_id() != worker_.get_id()) {
        worker_.join();
    }

    running_.store(false);
    interruptFlag_.store(false);

    std::cout << "[" << serviceName_ << "] Service stopped." << std::endl;
}

void AsyncService::restart() {
    std::cout << "[" << serviceName_ << "] Restarting service..." << std::endl;
    stop();
    start();
}

void AsyncService::handleInterrupt() {
    if (interruptFlag_.load()) {
        std::cout << "[" << serviceName_ << "] Interrupt signal received. Stopping service..." << std::endl;
        stop();
    }
}
