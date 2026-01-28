#pragma once

#include "nlohmann/json_fwd.hpp"

namespace vh::websocket {

using json = nlohmann::json;

struct WebSocketSession;

struct StatsHandler {
    static json vault(const json& payload, const WebSocketSession& session);
    static json fsCache(const WebSocketSession& session);
    static json httpCache(const WebSocketSession& session);
};

}
