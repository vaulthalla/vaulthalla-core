#include "services/ConnectionLifecycleManager.hpp"
#include <iostream>

namespace vh::services {

ConnectionLifecycleManager::ConnectionLifecycleManager(std::shared_ptr<auth::SessionManager> sessionManager)
    : sessionManager_(std::move(sessionManager)) {}

ConnectionLifecycleManager::~ConnectionLifecycleManager() {
    stop();
}

void ConnectionLifecycleManager::start() {
    running_ = true;
    lifecycleThread_ = std::thread(&ConnectionLifecycleManager::run, this);
    std::cout << "[LifecycleManager] Started." << std::endl;
}

void ConnectionLifecycleManager::stop() {
    running_ = false;
    if (lifecycleThread_.joinable()) {
        lifecycleThread_.join();
    }
    std::cout << "[LifecycleManager] Stopped." << std::endl;
}

void ConnectionLifecycleManager::run() {
    while (running_) {
        sweepActiveSessions();
        std::this_thread::sleep_for(SweepInterval);
    }
}

void ConnectionLifecycleManager::sweepActiveSessions() {
    auto sessions = sessionManager_->getActiveSessions(); // Expose activeSessions_ snapshot in SessionManager

    auto now = std::chrono::system_clock::now();

    for (const auto& [tokenStr, client] : sessions) {
        if (!client) continue;

        auto token = client->getToken();
        auto secondsLeft = client->getToken()->getTimeLeft();

        if (token->revoked) {
            std::cout << "[LifecycleManager] Token revoked. Closing session for user " << client->getUser()->id
                      << std::endl;
            client->sendControlMessage("token_revoked", {});
            client->closeConnection();
            sessionManager_->invalidateSession(tokenStr);
            continue;
        }

        if (secondsLeft <= 0) {
            std::cout << "[LifecycleManager] Token expired. Closing session for user " << client->getUser()->id
                      << std::endl;
            client->sendControlMessage("token_expired", {});
            client->closeConnection();
            sessionManager_->invalidateSession(tokenStr);
            continue;
        }

        if (secondsLeft <= 10) {
            client->sendControlMessage("token_refresh_urgent", {{"deadline_ms", 10000}});
        } else if (secondsLeft <= 300) {
            client->sendControlMessage("token_refresh_requested", {{"deadline_ms", 300000}});
        }
    }
}

} // namespace vh::services
