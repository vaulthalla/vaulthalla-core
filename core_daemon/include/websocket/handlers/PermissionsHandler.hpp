#pragma once

#include <nlohmann/json_fwd.hpp>

namespace vh::websocket {

using json = nlohmann::json;
class WebSocketSession;

struct PermissionsHandler {

    static void handleAddRole(const json& msg, WebSocketSession& session);
    static void handleDeleteRole(const json& msg, WebSocketSession& session);
    static void handleUpdateRole(const json& msg, WebSocketSession& session);
    static void handleGetRole(const json& msg, WebSocketSession& session);
    static void handleGetRoleByName(const json& msg, WebSocketSession& session);
    static void handleListRoles(const json& msg, WebSocketSession& session);

    static void handleGetPermission(const json& msg, WebSocketSession& session);
    static void handleGetPermissionByName(const json& msg, WebSocketSession& session);
    static void handleListPermissions(const json& msg, WebSocketSession& session);

};

}
