#include "websocket/handlers/AuthHandler.hpp"
#include "auth/AuthManager.hpp"
#include "auth/Client.hpp"
#include "websocket/WebSocketSession.hpp"
#include <iostream>

#include "types/User.hpp"
#include "database/Queries/UserQueries.hpp"

namespace vh::websocket {

AuthHandler::AuthHandler(const std::shared_ptr<auth::AuthManager>& authManager)
    : authManager_(authManager), sessionManager_(authManager->sessionManager()) {
    if (!authManager_) throw std::invalid_argument("AuthManager cannot be null");
}

void AuthHandler::handleLogin(const json& msg, WebSocketSession& session) const {
    try {
        json payload = msg.at("payload");
        const auto username = payload.at("name").get<std::string>();
        const auto password = payload.at("password").get<std::string>();

        const auto client = authManager_->loginUser(username, password, session.shared_from_this());
        if (!client) throw std::runtime_error("Invalid email or password or user not found");

        auto user = client->getUser();
        std::string token = client->getRawToken();

        // Bind user to WebSocketSession
        session.setAuthenticatedUser(user);

        const json data = {{"token", token}, {"user", *user}};

        const json response = {{"command", "auth.login.response"},
                         {"status", "ok"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"data", data}};

        session.send(response);

        std::cout << "[AuthHandler] User '" << username << "' logged in." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[AuthHandler] handleLogin error: " << e.what() << std::endl;

        const json response = {{"command", "auth.login.response"}, {"status", "error"}, {"error", e.what()}};

        session.send(response);
    }
}

void AuthHandler::handleRegister(const json& msg, WebSocketSession& session) const {
    try {
        json payload = msg.at("payload");
        const auto name = payload.at("name").get<std::string>();
        const auto email = payload.at("email").get<std::string>();
        const auto password = payload.at("password").get<std::string>();
        const auto isActive = payload.at("is_active").get<bool>();

        auto user = std::make_shared<types::User>(name, email, isActive);
        const auto client = authManager_->registerUser(user, password, session.shared_from_this());
        user = client->getUser();
        std::string token = client->getRawToken();

        // Bind user to WebSocketSession
        session.setAuthenticatedUser(user);

        const json data = {{"token", token}, {"user", *user}};

        const json response = {{"command", "auth.register.response"},
                         {"status", "ok"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"data", data}};

        session.send(response);

        std::cout << "[AuthHandler] User '" << name << "' registered." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[AuthHandler] handleRegister error: " << e.what() << std::endl;

        const json response = {{"command", "auth.register.response"},
                         {"status", "error"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"error", e.what()}};

        session.send(response);
    }
}

void AuthHandler::handleUpdateUser(const json& msg, WebSocketSession& session) const {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user) throw std::runtime_error("User not authenticated");

        const auto& payload = msg.at("payload");
        user->updateUser(payload);
        authManager_->updateUser(user);

        const json data = {{"user", *user}};

        const json response = {{"command", "auth.user.update.response"},
                         {"status", "ok"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"data", data}};

        session.send(response);

        std::cout << "[AuthHandler] Updated user '" << user->name << "'." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[AuthHandler] handleUpdateUser error: " << e.what() << std::endl;

        const json response = {{"command", "auth.user.update.response"},
                         {"status", "error"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"error", e.what()}};

        session.send(response);
    }
}

void AuthHandler::handleChangePassword(const json& msg, WebSocketSession& session) const {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user) throw std::runtime_error("User not authenticated");

        json payload = msg.at("payload");
        const auto oldPassword = payload.at("old_password").get<std::string>();
        const auto newPassword = payload.at("new_password").get<std::string>();

        authManager_->changePassword(user->name, oldPassword, newPassword);

        const json response = {{"command", "auth.user.change_password.response"},
                         {"status", "ok"},
                         {"requestId", msg.at("requestId").get<std::string>()}};

        session.send(response);

        std::cout << "[AuthHandler] Changed password for user '" << user->name << "'." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[AuthHandler] handleChangePassword error: " << e.what() << std::endl;

        const json response = {{"command", "auth.user.change_password.response"},
                         {"status", "error"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"error", e.what()}};

        session.send(response);
    }
}

void AuthHandler::handleGetUser(const json& msg, WebSocketSession& session) const {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user) throw std::runtime_error("User not authenticated");

        const auto requestId = msg.at("payload").at("id").get<unsigned int>();

        if (user->id != requestId && !user->canManageRoles())
            throw std::runtime_error("Permission denied: Only admins can fetch user data");

        const auto requestedUser = database::UserQueries::getUserById(requestId);

        const json data = {{"user", *requestedUser}};

        const json response = {{"command", "auth.user.get.response"},
                         {"status", "ok"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"data", data}};

        session.send(response);

        std::cout << "[AuthHandler] Fetched user data for '" << requestedUser->name << "'." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[AuthHandler] handleGetUser error: " << e.what() << std::endl;

        const json response = {{"command", "auth.user.get.response"},
                         {"status", "error"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"error", e.what()}};

        session.send(response);
    }
}


void AuthHandler::handleRefresh(const json& msg, WebSocketSession& session) const {
    try {
        const auto client = authManager_->validateRefreshToken(session.getRefreshToken(), session.shared_from_this());

        if (!client) throw std::runtime_error("Session not found");

        client->refreshToken();

        const json data = {{"token", client->getRawToken()}, {"user", *client->getUser()}};

        const json response = {{"command", "auth.refresh.response"},
                         {"status", "ok"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"data", data}};

        session.send(response);

        std::cout << "[AuthHandler] User '" << client->getUser()->name << "' refreshed session." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[AuthHandler] handleRefresh error: " << e.what() << std::endl;

        const json response = {{"command", "auth.refresh.response"},
                         {"status", "error"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"error", e.what()}};

        session.send(response);
    }
}

void AuthHandler::handleLogout(const json& msg, WebSocketSession& session) const {
    try {
        const auto refreshToken = session.getRefreshToken();

        sessionManager_->invalidateSession(session.getUUID());

        // Clear bound user in WebSocketSession
        session.setAuthenticatedUser(nullptr);

        const json response = {
            {"command", "auth.logout.response"},
            {"status", "ok"},
            {"requestId", msg.at("requestId").get<std::string>()},
        };

        session.send(response);

        std::cout << "[AuthHandler] Session '" << refreshToken << "' logged out." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[AuthHandler] handleLogout error: " << e.what() << std::endl;

        const json response = {{"command", "auth.logout.response"},
                         {"status", "error"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"error", e.what()}};

        session.send(response);
    }
}

void AuthHandler::handleListUsers(const json& msg, WebSocketSession& session) {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user->canManageRoles()) throw std::runtime_error("Permission denied: Only admins can list users");
        const auto users = database::UserQueries::listUsers();

        const json data = {
            {"users", types::to_json(users)}
        };

        const json response = {{"command", "auth.users.list.response"},
                         {"status", "ok"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"data", data}};

        session.send(response);

        std::cout << "[AuthHandler] Listed " << users.size() << " users." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[AuthHandler] handleListUsers error: " << e.what() << std::endl;

        json response = {{"command", "auth.users.list.response"},
                         {"status", "error"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"error", e.what()}};

        session.send(response);
    }
}

void AuthHandler::isUserAuthenticated(const json& msg, WebSocketSession& session) const {
    try {
        const auto token = msg.at("token").get<std::string>();
        const auto client = sessionManager_->getClientSession(token);

        const json data = {{"isAuthenticated", client && client->isAuthenticated()},
                     {"user", *client->getUser()}};

        const json response = {{"command", "auth.isAuthenticated.response"},
                         {"status", "ok"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"data", data}};

        session.send(response);

        std::cout << "[AuthHandler] User authentication check: "
                  << (client->isAuthenticated() ? "Authenticated" : "Unauthenticated") << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[AuthHandler] isUserAuthenticated error: " << e.what() << std::endl;
    }
}

} // namespace vh::websocket
