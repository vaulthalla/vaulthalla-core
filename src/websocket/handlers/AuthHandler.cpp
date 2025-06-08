#include "websocket/handlers/AuthHandler.hpp"
#include "websocket/WebSocketSession.hpp"
#include "auth/AuthManager.hpp"
#include "auth/SessionManager.hpp"
#include "auth/TokenValidator.hpp"
#include <iostream>

namespace vh::websocket {

    AuthHandler::AuthHandler(vh::auth::SessionManager& sessionManager,
                             vh::auth::AuthManager& authManager,
                             vh::auth::TokenValidator& tokenValidator)
            : sessionManager_(sessionManager),
              authManager_(authManager),
              tokenValidator_(tokenValidator) {}

    void AuthHandler::handleLogin(const json& msg, WebSocketSession& session) {
        try {
            std::string username = msg.at("username").get<std::string>();
            std::string password = msg.at("password").get<std::string>();

            auto user = authManager_.loginUser(username, password);
            std::string token = sessionManager_.createSession(user);

            // Bind user to WebSocketSession
            session.setAuthenticatedUser(user);

            json response = {
                    {"command", "auth.login.response"},
                    {"status", "ok"},
                    {"token", token},
                    {"username", user->getUsername()},
                    {"publicKey", user->getPublicKey()}
            };

            session.send(response);

            std::cout << "[AuthHandler] User '" << username << "' logged in.\n";
        } catch (const std::exception& e) {
            std::cerr << "[AuthHandler] handleLogin error: " << e.what() << "\n";

            json response = {
                    {"command", "auth.login.response"},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void AuthHandler::handleRefresh(const json& msg, WebSocketSession& session) {
        try {
            std::string token = msg.at("token").get<std::string>();

            if (!tokenValidator_.validateToken(token)) {
                throw std::runtime_error("Invalid token");
            }

            auto user = sessionManager_.getUserForSession(token);
            if (!user) {
                throw std::runtime_error("Session not found");
            }

            // Re-bind user to session
            session.setAuthenticatedUser(user);

            json response = {
                    {"command", "auth.refresh.response"},
                    {"status", "ok"},
                    {"token", token},
                    {"username", user->getUsername()},
                    {"publicKey", user->getPublicKey()}
            };

            session.send(response);

            std::cout << "[AuthHandler] User '" << user->getUsername() << "' refreshed session.\n";
        } catch (const std::exception& e) {
            std::cerr << "[AuthHandler] handleRefresh error: " << e.what() << "\n";

            json response = {
                    {"command", "auth.refresh.response"},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void AuthHandler::handleLogout(const json& msg, WebSocketSession& session) {
        try {
            std::string token = msg.at("token").get<std::string>();

            sessionManager_.invalidateSession(token);

            // Clear bound user in WebSocketSession
            session.setAuthenticatedUser(nullptr);

            json response = {
                    {"command", "auth.logout.response"},
                    {"status", "ok"}
            };

            session.send(response);

            std::cout << "[AuthHandler] Session '" << token << "' logged out.\n";
        } catch (const std::exception& e) {
            std::cerr << "[AuthHandler] handleLogout error: " << e.what() << "\n";

            json response = {
                    {"command", "auth.logout.response"},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

} // namespace vh::websocket
