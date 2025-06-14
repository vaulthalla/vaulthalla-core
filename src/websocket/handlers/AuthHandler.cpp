#include "websocket/handlers/AuthHandler.hpp"
#include "websocket/WebSocketSession.hpp"
#include "auth/AuthManager.hpp"
#include "auth/Client.hpp"
#include <iostream>

namespace vh::websocket {

    AuthHandler::AuthHandler(const std::shared_ptr<vh::auth::AuthManager> &authManager)
            : authManager_(authManager),
              sessionManager_(authManager->sessionManager()) {
        if (!authManager_) throw std::invalid_argument("AuthManager cannot be null");
    }

    void AuthHandler::handleLogin(const json& msg, WebSocketSession& session) {
        try {
            json payload = msg.at("payload");
            std::string email = payload.at("email").get<std::string>();
            std::string password = payload.at("password").get<std::string>();

            auto client = authManager_->loginUser(email, password, session.shared_from_this());
            if (!client) throw std::runtime_error("Invalid email or password or user not found");

            auto user = client->getUser();
            std::string token = client->getRawToken();

            // Bind user to WebSocketSession
            session.setAuthenticatedUser(user);

            json data = {
                    {"token", token},
                    {"user", *user}
            };

            json response = {
                    {"command", "auth.login.response"},
                    {"status", "ok"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"data", data}
            };

            session.send(response);

            std::cout << "[AuthHandler] User '" << email << "' logged in.\n";
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

    void AuthHandler::handleRegister(const json& msg, WebSocketSession& session) {
        try {
            json payload = msg.at("payload");
            std::string name = payload.at("name").get<std::string>();
            std::string email = payload.at("email").get<std::string>();
            std::string password = payload.at("password").get<std::string>();

            auto client = authManager_->registerUser(name, email, password, session.shared_from_this());
            auto user = client->getUser();
            std::string token = client->getRawToken();

            // Bind user to WebSocketSession
            session.setAuthenticatedUser(user);

            json data = {
                    {"token", token},
                    {"user", *user}
            };

            json response = {
                    {"command", "auth.register.response"},
                    {"status", "ok"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"data", data}
            };

            session.send(response);

            std::cout << "[AuthHandler] User '" << name << "' registered.\n";
        } catch (const std::exception& e) {
            std::cerr << "[AuthHandler] handleRegister error: " << e.what() << "\n";

            json response = {
                    {"command", "auth.register.response"},
                    {"status", "error"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void AuthHandler::handleRefresh(const json& msg, WebSocketSession& session) {
        try {
            std::cout << "Refresh Token: " << session.getRefreshToken() << "\n";
            std::cout << "Refresh Token from Cookie: " << session.getRefreshTokenFromCookie() << "\n";
            auto client = authManager_->validateRefreshToken(session.getRefreshTokenFromCookie(),
                                                             session.shared_from_this());

            if (!client) throw std::runtime_error("Session not found");

            client->refreshToken();

            json data = {
                    {"token", client->getRawToken()},
                    {"user", *client->getUser()}
            };

            json response = {
                    {"command", "auth.refresh.response"},
                    {"status", "ok"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"data", data}
            };

            session.send(response);

            std::cout << "[AuthHandler] User '" << client->getUser()->email << "' refreshed session.\n";
        } catch (const std::exception& e) {
            std::cerr << "[AuthHandler] handleRefresh error: " << e.what() << "\n";

            json response = {
                    {"command", "auth.refresh.response"},
                    {"status", "error"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void AuthHandler::handleLogout(const json& msg, WebSocketSession& session) {
        try {
            std::string token = msg.at("token").get<std::string>();

            sessionManager_->invalidateSession(token);

            // Clear bound user in WebSocketSession
            session.setAuthenticatedUser(nullptr);

            json response = {
                    {"command", "auth.logout.response"},
                    {"status", "ok"},
                    {"requestId", msg.at("requestId").get<std::string>()},
            };

            session.send(response);

            std::cout << "[AuthHandler] Session '" << token << "' logged out.\n";
        } catch (const std::exception& e) {
            std::cerr << "[AuthHandler] handleLogout error: " << e.what() << "\n";

            json response = {
                    {"command", "auth.logout.response"},
                    {"status", "error"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void AuthHandler::isUserAuthenticated(const json& msg, WebSocketSession& session) {
        try {
            auto token = msg.at("token").get<std::string>();
            auto client = sessionManager_->getClientSession(token);

            json data = {
                    {"isAuthenticated", client && client->isAuthenticated()},
                    {"user", client ? *client->getUser() : json::object()}
            };

            json response = {
                    {"command", "auth.isAuthenticated.response"},
                    {"status", "ok"},
                    {"requestId", msg.at("requestId").get<std::string>()},
                    {"data", data}
            };

            session.send(response);

            std::cout << "[AuthHandler] User authentication check: "
                      << (client->isAuthenticated() ? "Authenticated" : "Unauthenticated") << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[AuthHandler] isUserAuthenticated error: " << e.what() << "\n";
        }
    }

} // namespace vh::websocket
