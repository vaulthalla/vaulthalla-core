#pragma once

#include <nlohmann/json.hpp>

namespace vh::websocket {

using json = nlohmann::json;

class WebSocketSession;

class ShareHandler {
  public:
    ShareHandler() = default;

    void handleCreateLink(const json& msg, WebSocketSession& session);
    void handleResolveLink(const json& msg, WebSocketSession& session);
};

} // namespace vh::websocket
