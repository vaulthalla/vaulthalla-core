#pragma once

#include <nlohmann/json_fwd.hpp>

namespace vh::websocket {

using json = nlohmann::json;
class WebSocketSession;

struct SettingsHandler {
    static void handleGetSettings(const json& msg, WebSocketSession& session);
    static void handleUpdateSettings(const json& msg, WebSocketSession& session);
};

}
