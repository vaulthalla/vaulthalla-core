#include "protocols/websocket/handlers/AuthHandler.hpp"
#include "auth/AuthManager.hpp"
#include "auth/Client.hpp"
#include "protocols/websocket/WebSocketSession.hpp"
#include "types/User.hpp"
#include "database/Queries/UserQueries.hpp"
#include "logging/LogRegistry.hpp"

using namespace vh::websocket;
using namespace vh::auth;
using namespace vh::types;
using namespace vh::database;
using namespace vh::logging;

AuthHandler::AuthHandler(const std::shared_ptr<AuthManager>& authManager)
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

        LogRegistry::auth()->info("[AuthHandler] User '{}' logged in successfully.", username);
    } catch (const std::exception& e) {
        LogRegistry::auth()->error("[AuthHandler] handleLogin error: {}", e.what());

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

        auto user = std::make_shared<User>(name, email, isActive);
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

        LogRegistry::auth()->info("[AuthHandler] User '{}' registered successfully.", name);
    } catch (const std::exception& e) {
        LogRegistry::auth()->error("[AuthHandler] handleRegister error: {}", e.what());

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

        LogRegistry::auth()->info("[AuthHandler] Updated user '{}' data.", user->name);
    } catch (const std::exception& e) {
        LogRegistry::auth()->error("[AuthHandler] handleUpdateUser error: {}", e.what());

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

        LogRegistry::auth()->info("[AuthHandler] User '{}' changed password successfully.", user->name);
    } catch (const std::exception& e) {
        LogRegistry::auth()->error("[AuthHandler] handleChangePassword error: {}", e.what());

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

        const auto requestedUser = UserQueries::getUserById(requestId);

        const json data = {{"user", *requestedUser}};

        const json response = {{"command", "auth.user.get.response"},
                         {"status", "ok"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"data", data}};

        session.send(response);

        LogRegistry::auth()->info("[AuthHandler] Fetched user '{}' data.", requestedUser->name);
    } catch (const std::exception& e) {
        LogRegistry::auth()->error("[AuthHandler] handleGetUser error: {}", e.what());

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

        LogRegistry::auth()->info("[AuthHandler] Session '{}' refreshed successfully.", session.getRefreshToken());
    } catch (const std::exception& e) {
        LogRegistry::auth()->error("[AuthHandler] handleRefresh error: {}", e.what());

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

        LogRegistry::auth()->info("[AuthHandler] Session '{}' logged out successfully.", refreshToken);
    } catch (const std::exception& e) {
        LogRegistry::auth()->error("[AuthHandler] handleLogout error: {}", e.what());

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
        const auto users = UserQueries::listUsers();

        const json data = {
            {"users", to_json(users)}
        };

        const json response = {{"command", "auth.users.list.response"},
                         {"status", "ok"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"data", data}};

        session.send(response);

        LogRegistry::auth()->info("[AuthHandler] Fetched list of users successfully.");
    } catch (const std::exception& e) {
        LogRegistry::auth()->error("[AuthHandler] handleListUsers error: {}", e.what());

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

        LogRegistry::auth()->info("[AuthHandler] Checked authentication status for token '{}'.", token);
    } catch (const std::exception& e) {
        LogRegistry::auth()->error("[AuthHandler] isUserAuthenticated error: {}", e.what());

        const json response = {{"command", "auth.isAuthenticated.response"},
                         {"status", "error"},
                         {"requestId", msg.at("requestId").get<std::string>()},
                         {"error", e.what()}};

        session.send(response);
    }
}
