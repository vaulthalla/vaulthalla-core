#include "protocols/websocket/handlers/AuthHandler.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "auth/AuthManager.hpp"
#include "auth/Client.hpp"
#include "database/Queries/PermsQueries.hpp"
#include "protocols/websocket/WebSocketSession.hpp"
#include "types/User.hpp"
#include "database/Queries/UserQueries.hpp"
#include "logging/LogRegistry.hpp"

using namespace vh::websocket;
using namespace vh::auth;
using namespace vh::types;
using namespace vh::database;
using namespace vh::logging;

json AuthHandler::login(const json& payload, WebSocketSession& session) {
    const auto username = payload.at("name").get<std::string>();
    const auto password = payload.at("password").get<std::string>();

    const auto client = ServiceDepsRegistry::instance().authManager->loginUser(username, password, session.shared_from_this());
    if (!client) throw std::runtime_error("Invalid email or password or user not found");

    session.setAuthenticatedUser(client->getUser());
    return {{"token", client->getRawToken()}, {"user", *client->getUser()}};
}

json AuthHandler::registerUser(const json& payload, WebSocketSession& session) {
    const auto name = payload.at("name").get<std::string>();
    const auto email = payload.at("email").get<std::string>();
    const auto password = payload.at("password").get<std::string>();
    const auto isActive = payload.at("is_active").get<bool>();
    const auto role = payload.at("role").get<std::string>();

    const auto userRole = PermsQueries::getRoleByName(role);

    auto user = std::make_shared<User>(name, email, isActive);
    const auto client = ServiceDepsRegistry::instance().authManager->registerUser(user, password, session.shared_from_this());
    user = client->getUser();
    std::string token = client->getRawToken();

    // Bind user to WebSocketSession
    session.setAuthenticatedUser(user);

    return {{"token", token}, {"user", *user}};
}

json AuthHandler::updateUser(const json& payload, const WebSocketSession& session) {
    const auto user = session.getAuthenticatedUser();
    if (!user) throw std::runtime_error("User not authenticated");

    user->updateUser(payload);
    ServiceDepsRegistry::instance().authManager->updateUser(user);

    return {{"user", *user}};
}

json AuthHandler::changePassword(const json& payload, const WebSocketSession& session) {
    const auto user = session.getAuthenticatedUser();
    if (!user) throw std::runtime_error("User not authenticated");

    const auto oldPassword = payload.at("old_password").get<std::string>();
    const auto newPassword = payload.at("new_password").get<std::string>();

    ServiceDepsRegistry::instance().authManager->changePassword(user->name, oldPassword, newPassword);

    return {};
}

json AuthHandler::getUser(const json& payload, const WebSocketSession& session) {
    const auto user = session.getAuthenticatedUser();
    if (!user) throw std::runtime_error("User not authenticated");

    const auto userId = payload.at("id").get<unsigned int>();

    if (user->id != userId && !user->canManageRoles()) throw std::runtime_error(
        "Permission denied: Only admins can fetch user data");

    const auto requestedUser = UserQueries::getUserById(userId);

    return {{"user", *requestedUser}};
}

json AuthHandler::refresh(WebSocketSession& session) {
    const auto client = ServiceDepsRegistry::instance().authManager->validateRefreshToken(session.getRefreshToken(), session.shared_from_this());
    if (!client) throw std::runtime_error("No client attached to current websocket session. Please reauthenticate.");
    client->refreshToken();
    return {{"token", client->getRawToken()}, {"user", *client->getUser()}};
}

json AuthHandler::logout(WebSocketSession& session) {
    const auto refreshToken = session.getRefreshToken();
    ServiceDepsRegistry::instance().authManager->sessionManager()->invalidateSession(session.getUUID());
    session.setAuthenticatedUser(nullptr);
    return {};
}

json AuthHandler::listUsers(const WebSocketSession& session) {
    if (const auto user = session.getAuthenticatedUser();
        !user->canManageRoles()) throw std::runtime_error("Permission denied: Only admins can list users");
    const auto users = UserQueries::listUsers();
    return {{"users", to_json(users)}};
}

json AuthHandler::isUserAuthenticated(const std::string& token, const WebSocketSession& session)  {
    const auto client = ServiceDepsRegistry::instance().authManager->sessionManager()->getClientSession(session.getUUID());
    if (!client) throw std::runtime_error("No access token attached to current websocket session. Please reauthenticate.");

    const auto user = client->getUser();
    if (!user) throw std::runtime_error("User not found for token: " + token);

    const auto isAuthenticated = client->isAuthenticated() && client->validateToken(token);
    json data = {{"isAuthenticated", isAuthenticated}};
    if (isAuthenticated) data["user"] = *user;
    return data;
}

json AuthHandler::getUserByName(const json& payload, const WebSocketSession& session) {
    const auto user = session.getAuthenticatedUser();
    if (!user) throw std::runtime_error("User not authenticated");

    const auto name = payload.at("name").get<std::string>();
    if (user->name != name && !user->canManageRoles()) throw std::runtime_error(
        "Permission denied: Only admins can fetch user by name");

    const auto retUser = UserQueries::getUserByName(name);
    if (!retUser) throw std::runtime_error("User not found");

    return {{"user", *retUser}};
}

json AuthHandler::doesAdminHaveDefaultPassword() {
    return {{"isDefault", UserQueries::adminPasswordIsDefault()}};
}