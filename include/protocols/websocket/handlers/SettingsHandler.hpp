#pragma once

#include <nlohmann/json_fwd.hpp>

namespace vh::websocket {

using json = nlohmann::json;
struct WebSocketSession;

struct SettingsHandler {
    static json get(const WebSocketSession& session);
    static json update(const json& payload, const WebSocketSession& session);
};

}
