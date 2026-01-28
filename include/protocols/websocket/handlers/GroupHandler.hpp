#pragma once

#include <nlohmann/json_fwd.hpp>

using json = nlohmann::json;

namespace vh::websocket {

struct WebSocketSession;

struct GroupHandler {
    static json add(const json& payload, const WebSocketSession& session);
    static json remove(const json& payload, const WebSocketSession& session);
    static json addMember(const json& payload, const WebSocketSession& session);
    static json removeMember(const json& payload, const WebSocketSession& session);
    static json update(const json& payload, const WebSocketSession& session);
    static json get(const json& payload, const WebSocketSession& session);
    static json getByName(const json& payload, const WebSocketSession& session);
    static json listByUser(const json& payload, const WebSocketSession& session);
    static json list(const WebSocketSession& session);
};

}
