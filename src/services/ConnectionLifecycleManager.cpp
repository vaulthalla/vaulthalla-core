#include "services/ConnectionLifecycleManager.hpp"
#include "auth/SessionManager.hpp"
#include "auth/Client.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "services/LogRegistry.hpp"

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
    while (running_ && !interruptFlag_.load()) {
        sweepActiveSessions();
        std::this_thread::sleep_for(SweepInterval);
    }
}

void ConnectionLifecycleManager::sweepActiveSessions() const {
    const auto sessions = sessionManager_->getActiveSessions(); // Expose activeSessions_ snapshot in SessionManager

    for (const auto& [tokenStr, client] : sessions) {
        if (!client) continue;

        const auto token = client->getToken();
        const auto secondsLeft = client->getToken()->getTimeLeft();

        if (token->revoked) {
            LogRegistry::ws()->info("[LifecycleManager] Token revoked. Closing session for user {}", client->getUser()->id);
            client->sendControlMessage("token_revoked", {});
            client->closeConnection();
            sessionManager_->invalidateSession(tokenStr);
            continue;
        }

        if (secondsLeft <= 0) {
            LogRegistry::ws()->info("[LifecycleManager] Token expired. Closing session for user {}", client->getUser()->id);
            client->sendControlMessage("token_expired", {});
            client->closeConnection();
            sessionManager_->invalidateSession(tokenStr);
            continue;
        }

        if (secondsLeft <= 10)
            client->sendControlMessage("token_refresh_urgent", {{"deadline_ms", 10000}});
        else if (secondsLeft <= 300)
            client->sendControlMessage("token_refresh_requested", {{"deadline_ms", 300000}});
    }
}
