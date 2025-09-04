#include "services/ConnectionLifecycleManager.hpp"
#include "auth/SessionManager.hpp"
#include "auth/Client.hpp"
#include "auth/AuthManager.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "logging/LogRegistry.hpp"

using namespace vh::services;
using namespace vh::auth;
using namespace vh::logging;

ConnectionLifecycleManager::ConnectionLifecycleManager()
    : AsyncService("LifecycleManager"),
      sessionManager_(ServiceDepsRegistry::instance().authManager->sessionManager()) {}

ConnectionLifecycleManager::~ConnectionLifecycleManager() {
    stop();
}

void ConnectionLifecycleManager::runLoop() {
    LogRegistry::ws()->info("[LifecycleManager] Started connection lifecycle manager.");
    while (running_ && !interruptFlag_.load()) {
        sweepActiveSessions();
        std::this_thread::sleep_for(SWEEP_INTERVAL);
    }
}

void ConnectionLifecycleManager::sweepActiveSessions() const {
    for (const auto& [tokenStr, client] : sessionManager_->getActiveSessions()) {
        if (!client) continue;

        const auto token = client->getToken();

        if (client->connOpenedAt() + UNAUTHENTICATED_SESSION_TIMEOUT < std::chrono::system_clock::now() && !token) {
            LogRegistry::ws()->debug("[LifecycleManager] Closing unauthenticated session (no token) opened at {}",
                                    std::chrono::system_clock::to_time_t(client->connOpenedAt()));
            client->sendControlMessage("unauthenticated_session_timeout", {});
            client->closeConnection();
            sessionManager_->invalidateSession(tokenStr);
            continue;
        }

        if (!token || token->revoked) {
            if (const auto user = client->getUser()) {
                LogRegistry::ws()->debug("[LifecycleManager] Token revoked. Closing session for user {}", user->id);
                client->sendControlMessage("token_revoked", {});
            }
            client->closeConnection();
            sessionManager_->invalidateSession(tokenStr);
            continue;
        }

        const auto secondsLeft = token->getTimeLeft();

        if (secondsLeft <= 0) {
            if (const auto user = client->getUser()) {
                LogRegistry::ws()->debug("[LifecycleManager] Token expired. Closing session for user {}", user->id);
                client->sendControlMessage("token_expired", {});
            }
            client->closeConnection();
            sessionManager_->invalidateSession(tokenStr);
            continue;
        }

        // sendControlMessage requires a user to be set
        if (client->getUser()) {
            if (secondsLeft <= 10)
                client->sendControlMessage("token_refresh_urgent", {{"deadline_ms", 10000}});
            else if (secondsLeft <= 300)
                client->sendControlMessage("token_refresh_requested", {{"deadline_ms", 300000}});
        }
    }
}
