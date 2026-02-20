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

    std::chrono::seconds sweep_interval_{30};
    std::chrono::seconds unauthenticated_session_timeout_{60};
    std::chrono::minutes idle_timeout_{30};
};

} // namespace vh::services