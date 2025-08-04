#pragma once

#include <memory>
#include <atomic>
#include <thread>
#include <string>

namespace vh::services {

class ServiceManager;

class AsyncService {
public:
    explicit AsyncService(const std::string& serviceName);

    virtual ~AsyncService();

    virtual void start();

    virtual void stop();

    virtual void restart();

    [[nodiscard]] bool isRunning() const { return running_.load(); }

    void handleInterrupt();

protected:
    std::string serviceName_;
    std::atomic<bool> running_{false};
    std::atomic<bool> interruptFlag_{false};
    std::thread worker_;

    virtual void runLoop() = 0;
};

}