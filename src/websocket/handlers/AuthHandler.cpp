#include "websocket/handlers/AuthHandler.hpp"
#include "websocket/WebSocketSession.hpp"
#include "auth/AuthManager.hpp"
#include <iostream>

namespace vh::websocket {

    AuthHandler::AuthHandler(const std::shared_ptr<vh::auth::AuthManager> &authManager)
            : authManager_(authManager),
              sessionManager_(authManager->sessionManager()) {
        if (!authManager_) throw std::invalid_argument("AuthManager cannot be null");
    }

    void AuthHandler::handleLogin(const json& msg, WebSocketSession& session) {
        try {
            std::string username = msg.at("username").get<std::string>();
            std::string password = msg.at("password").get<std::string>();

            auto user = authManager_->loginUser(username, password);
            std::string token = sessionManager_->createSession(session.shared_from_this(), user);

            // Bind user to WebSocketSession
            session.setAuthenticatedUser(user);

            json response = {
                    {"command", "auth.login.response"},
                    {"status", "ok"},
                    {"token", token},
                    {"user", *user}
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

    void AuthHandler::handleRegister(const json& msg, WebSocketSession& session) {
        try {
            json data = msg.at("payload");
            std::string name = data.at("name").get<std::string>();
            std::string email = data.at("email").get<std::string>();
            std::string password = data.at("password").get<std::string>();

            auto user = authManager_->registerUser(name, email, password);
            std::string token = sessionManager_->createSession(session.shared_from_this(), user);

            // Bind user to WebSocketSession
            session.setAuthenticatedUser(user);

            json response = {
                    {"command", "auth.register.response"},
                    {"status", "ok"},
                    {"token", token},
                    {"user", *user}
            };

            session.send(response);

            std::cout << "[AuthHandler] User '" << name << "' registered.\n";
        } catch (const std::exception& e) {
            std::cerr << "[AuthHandler] handleRegister error: " << e.what() << "\n";

            json response = {
                    {"command", "auth.register.response"},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void AuthHandler::handleRefresh(const json& msg, WebSocketSession& session) {
        try {
            std::string token = msg.at("token").get<std::string>();

            auto client = sessionManager_->getClientSession(token);

            if (!client) throw std::runtime_error("Session not found");

            client->refreshToken();

            json response = {
                    {"command", "auth.refresh.response"},
                    {"status", "ok"},
                    {"token", token},
                    {"user", *client->getUser()}
            };

            session.send(response);

            std::cout << "[AuthHandler] User '" << client->getUser()->email << "' refreshed session.\n";
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

            sessionManager_->invalidateSession(token);

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

    void AuthHandler::handleUnauthenticatedHello(WebSocketSession& session) {
        try {
            std::string greeting = "Shield Brother";

            json response = {
                    {"command", "auth.hello.response"},
                    {"status", "ok"},
                    {"message", "Hello, " + greeting + "!"}
            };

            session.send(response);

            std::cout << "[AuthHandler] Unauthenticated hello: " << greeting << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[AuthHandler] handleUnauthenticatedHello error: " << e.what() << "\n";
        }
    }

} // namespace vh::websocket
