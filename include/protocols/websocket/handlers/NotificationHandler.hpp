#pragma once

#include "protocols/websocket/WebSocketSession.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace vh::websocket {

using json = nlohmann::json;

class NotificationHandler {
  public:
    NotificationHandler() = default;

    void handleSubscribe(const json& msg, WebSocketSession& session);
    void handleUnsubscribe(const json& msg, WebSocketSession& session);

    // Server-side push API
    void pushNotification(WebSocketSession& session, const std::string& channel, const json& payload);
};

} // namespace vh::websocket
