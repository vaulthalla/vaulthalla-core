#pragma once

#include <nlohmann/json.hpp>
#include <memory>

namespace vh::auth {
    class SessionManager;
    class AuthManager;
    class TokenValidator;
}

namespace vh::websocket {

    using json = nlohmann::json;

    class WebSocketSession;

    class AuthHandler {
    public:
        AuthHandler(vh::auth::SessionManager& sessionManager,
                    vh::auth::AuthManager& authManager,
                    vh::auth::TokenValidator& tokenValidator);

        void handleLogin(const json& msg, WebSocketSession& session);
        void handleRefresh(const json& msg, WebSocketSession& session);
        void handleLogout(const json& msg, WebSocketSession& session);

    private:
        vh::auth::SessionManager& sessionManager_;
        vh::auth::AuthManager& authManager_;
        vh::auth::TokenValidator& tokenValidator_;
    };

} // namespace vh::websocket
