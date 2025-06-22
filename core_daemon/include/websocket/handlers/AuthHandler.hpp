#pragma once

#include <memory>
#include <nlohmann/json.hpp>

namespace vh::auth {
class SessionManager;
class AuthManager;
} // namespace vh::auth

namespace vh::websocket {

using json = nlohmann::json;

class WebSocketSession;

class AuthHandler {
  public:
    explicit AuthHandler(const std::shared_ptr<auth::AuthManager>& authManager);

    void handleLogin(const json& msg, WebSocketSession& session) const;
    void handleRegister(const json& msg, WebSocketSession& session) const;
    void handleRefresh(const json& msg, WebSocketSession& session) const;
    void handleLogout(const json& msg, WebSocketSession& session) const;
    void handleGetUser(const json& msg, WebSocketSession& session) const;
    void handleListUsers(const json& msg, WebSocketSession& session);
    void isUserAuthenticated(const json& msg, WebSocketSession& session) const;

  private:
    std::shared_ptr<auth::AuthManager> authManager_;
    std::shared_ptr<auth::SessionManager> sessionManager_;
};

} // namespace vh::websocket
