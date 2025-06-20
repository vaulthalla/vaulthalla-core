#include "websocket/handlers/NotificationBroadcastManager.hpp"
#include "websocket/WebSocketSession.hpp"
#include <iostream>

namespace vh::websocket {

void NotificationBroadcastManager::registerSession(std::shared_ptr<WebSocketSession> session) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    sessions_.insert(session);

    std::cout << "[NotificationBroadcastManager] Registered new session.\n";
}

void NotificationBroadcastManager::unregisterSession(std::shared_ptr<WebSocketSession> session) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    sessions_.erase(session);

    std::cout << "[NotificationBroadcastManager] Unregistered session.\n";
}

void NotificationBroadcastManager::broadcastNotification(const std::string& channel, const nlohmann::json& payload) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);

    std::cout << "[NotificationBroadcastManager] Broadcasting to channel '" << channel << "' to " << sessions_.size()
              << " session(s).\n";

    for (auto& session : sessions_) {
        if (session->isSubscribedTo(channel)) {
            nlohmann::json message = {{"command", "notification.push"}, {"channel", channel}, {"payload", payload}};

            session->send(message);
        }
    }
}

} // namespace vh::websocket
