#pragma once

#include <nlohmann/json_fwd.hpp>

using json = nlohmann::json;

namespace vh::websocket {

struct WebSocketSession;

struct GroupHandler {
    static void handleCreateGroup(const json& msg, WebSocketSession& session);
    static void handleDeleteGroup(const json& msg, WebSocketSession& session);
    static void handleAddMemberToGroup(const json& msg, WebSocketSession& session);
    static void handleRemoveMemberFromGroup(const json& msg, WebSocketSession& session);
    static void handleUpdateGroup(const json& msg, WebSocketSession& session);
    static void handleListGroups(const json& msg, WebSocketSession& session);
    static void handleGetGroup(const json& msg, WebSocketSession& session);
    static void handleGetGroupByName(const json& msg, WebSocketSession& session);
    static void handleListGroupsByUser(const json& msg, WebSocketSession& session);
};

}
