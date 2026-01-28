#pragma once

#include <nlohmann/json_fwd.hpp>

namespace vh::websocket {

using json = nlohmann::json;

class WebSocketSession;

struct APIKeyHandler {
    static json add(const json& payload, const WebSocketSession& session);
    static json remove(const json& payload, const WebSocketSession& session);
    static json list(const WebSocketSession& session);
    static json get(const json& payload, const WebSocketSession& session);
};

} // namespace vh::websocket
