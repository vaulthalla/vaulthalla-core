#pragma once

#include "auth/Client.hpp"
#include "auth/SessionManager.hpp"
#include <atomic>
#include <chrono>
#include <thread>

namespace vh::services {

class ConnectionLifecycleManager {
  public:
    explicit ConnectionLifecycleManager(std::shared_ptr<auth::SessionManager> sessionManager);

    ~ConnectionLifecycleManager();

    void start();

    void stop();

  private:
    void run();

    void sweepActiveSessions();

    std::shared_ptr<auth::SessionManager> sessionManager_;
    std::thread lifecycleThread_;
    std::atomic<bool> running_{false};

    constexpr static std::chrono::milliseconds SweepInterval{1000}; // 1 second sweep interval
};

} // namespace vh::services