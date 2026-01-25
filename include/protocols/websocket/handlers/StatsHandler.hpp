#pragma once

#include "nlohmann/json_fwd.hpp"

namespace vh::websocket {

using json = nlohmann::json;

class WebSocketSession;

struct StatsHandler {
    static void handleVaultStats(const json& msg, WebSocketSession& session);
};

}
