#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace vh::websocket {

using json = nlohmann::json;

struct WebSocketSession;

struct AuthHandler {
    static json login(const json& payload, WebSocketSession& session);
    static json registerUser(const json& payload, WebSocketSession& session);
    static json updateUser(const json& payload, const WebSocketSession& session);
    static json changePassword(const json& payload, const WebSocketSession& session);
    static json getUser(const json& payload, const WebSocketSession& session);
    static json getUserByName(const json& payload, const WebSocketSession& session);

    static json isUserAuthenticated(const std::string& token, const WebSocketSession& session);

    static json listUsers(const WebSocketSession& session);
    static json refresh(WebSocketSession& session);
    static json logout(WebSocketSession& session);

    static json doesAdminHaveDefaultPassword();
};

}
