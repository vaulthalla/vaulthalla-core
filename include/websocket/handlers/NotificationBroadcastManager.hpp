#pragma once

#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_set>

namespace vh::websocket {
class WebSocketSession;

class NotificationBroadcastManager {
  public:
    NotificationBroadcastManager() = default;

    void registerSession(std::shared_ptr<WebSocketSession> session);
    void unregisterSession(std::shared_ptr<WebSocketSession> session);

    void broadcastNotification(const std::string& channel, const nlohmann::json& payload);

  private:
    std::unordered_set<std::shared_ptr<WebSocketSession>> sessions_{};
    std::mutex sessionsMutex_;
};

} // namespace vh::websocket
