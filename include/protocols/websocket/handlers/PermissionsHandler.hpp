#pragma once

#include <nlohmann/json_fwd.hpp>

namespace vh::websocket {

using json = nlohmann::json;
struct WebSocketSession;

struct PermissionsHandler {
    static json get(const json& payload, const WebSocketSession& session);
    static json getByName(const json& payload, const WebSocketSession& session);
    static json list(const WebSocketSession& session);
};

}
