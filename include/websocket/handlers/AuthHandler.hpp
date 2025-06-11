#pragma once

#include <nlohmann/json.hpp>
#include <memory>

namespace vh::auth {
    class SessionManager;
    class AuthManager;
}

namespace vh::websocket {

    using json = nlohmann::json;

    class WebSocketSession;

    class AuthHandler {
    public:
        explicit AuthHandler(const std::shared_ptr<vh::auth::AuthManager>& authManager);

        void handleLogin(const json& msg, WebSocketSession& session);
        void handleRegister(const json& msg, WebSocketSession& session);
        void handleRefresh(const json& msg, WebSocketSession& session);
        void handleLogout(const json& msg, WebSocketSession& session);
        void isUserAuthenticated(const json& msg, WebSocketSession& session);
        void handleUnauthenticatedHello(WebSocketSession& session);

    private:
        std::shared_ptr<vh::auth::AuthManager> authManager_;
        std::shared_ptr<vh::auth::SessionManager> sessionManager_;
    };

} // namespace vh::websocket
