#pragma once

#include <nlohmann/json_fwd.hpp>

namespace vh::websocket {

using json = nlohmann::json;
struct WebSocketSession;

struct RolesHandler {
    static json add(const json& payload, const WebSocketSession& session);
    static json remove(const json& payload, const WebSocketSession& session);
    static json update(const json& payload, const WebSocketSession& session);
    static json get(const json& payload, const WebSocketSession& session);
    static json getByName(const json& payload, const WebSocketSession& session);

    static json list(const WebSocketSession& session);
    static json listUserRoles(const WebSocketSession& session);
    static json listVaultRoles(const WebSocketSession& session);
};

}
