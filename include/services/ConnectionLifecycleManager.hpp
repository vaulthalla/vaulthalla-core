#pragma once

#include "AsyncService.hpp"

#include <chrono>

namespace vh::auth {
class SessionManager;
}

namespace vh::services {

class ConnectionLifecycleManager : public AsyncService {
  public:
    ConnectionLifecycleManager();
    ~ConnectionLifecycleManager() override;

  private:
    void runLoop() override;

    void sweepActiveSessions() const;

    std::shared_ptr<auth::SessionManager> sessionManager_;

    constexpr static std::chrono::seconds SWEEP_INTERVAL{15};
    constexpr static std::chrono::seconds UNAUTHENTICATED_SESSION_TIMEOUT{60};
};

} // namespace vh::services