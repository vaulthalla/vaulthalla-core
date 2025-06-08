#pragma once

#include "websocket/WebSocketSession.hpp"
#include "auth/AuthManager.hpp"
#include "auth/SessionManager.hpp"
#include "auth/TokenValidator.hpp"

#include <nlohmann/json.hpp>
#include <memory>

namespace vh::websocket {

    using json = nlohmann::json;

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
