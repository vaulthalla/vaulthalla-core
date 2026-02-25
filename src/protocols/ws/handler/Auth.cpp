#include "protocols/ws/handler/Auth.hpp"
#include "runtime/Deps.hpp"
#include "auth/Manager.hpp"
#include "auth/model/Client.hpp"
#include "database/queries/PermsQueries.hpp"
#include "protocols/ws/Session.hpp"
#include "identities/model/User.hpp"
#include "database/queries/UserQueries.hpp"

using namespace vh::protocols::ws::handler;
using namespace vh::auth;
using namespace vh::identities::model;
using namespace vh::database;

json Auth::login(const json& payload, Session& session) {
    const auto username = payload.at("name").get<std::string>();
    const auto password = payload.at("password").get<std::string>();

    const auto client = runtime::Deps::get().authManager->loginUser(username, password, session.shared_from_this());
    if (!client) throw std::runtime_error("Invalid email or password or user not found");

    session.setAuthenticatedUser(client->getUser());
    return {{"token", client->getRawToken()}, {"user", *client->getUser()}};
}

json Auth::registerUser(const json& payload, Session& session) {
    const auto name = payload.at("name").get<std::string>();
    const auto email = payload.at("email").get<std::string>();
    const auto password = payload.at("password").get<std::string>();
    const auto isActive = payload.at("is_active").get<bool>();
    const auto role = payload.at("role").get<std::string>();

    const auto userRole = PermsQueries::getRoleByName(role);

    auto user = std::make_shared<User>(name, email, isActive);
    const auto client = runtime::Deps::get().authManager->registerUser(user, password, session.shared_from_this());
    user = client->getUser();
    std::string token = client->getRawToken();

    // Bind user to WebSocketSession
    session.setAuthenticatedUser(user);

    return {{"token", token}, {"user", *user}};
}

json Auth::updateUser(const json& payload, const Session& session) {
    const auto user = session.getAuthenticatedUser();
    if (!user) throw std::runtime_error("User not authenticated");

    user->updateUser(payload);
    runtime::Deps::get().authManager->updateUser(user);

    return {{"user", *user}};
}

json Auth::changePassword(const json& payload, const Session& session) {
    const auto user = session.getAuthenticatedUser();
    if (!user) throw std::runtime_error("User not authenticated");

    const auto oldPassword = payload.at("old_password").get<std::string>();
    const auto newPassword = payload.at("new_password").get<std::string>();

    runtime::Deps::get().authManager->changePassword(user->name, oldPassword, newPassword);

    return {};
}

json Auth::getUser(const json& payload, const Session& session) {
    const auto user = session.getAuthenticatedUser();
    if (!user) throw std::runtime_error("User not authenticated");

    const auto userId = payload.at("id").get<unsigned int>();

    if (user->id != userId && !user->canManageRoles()) throw std::runtime_error(
        "Permission denied: Only admins can fetch user data");

    const auto requestedUser = UserQueries::getUserById(userId);

    return {{"user", *requestedUser}};
}

json Auth::refresh(Session& session) {
    const auto client = runtime::Deps::get().authManager->validateRefreshToken(session.getRefreshToken(), session.shared_from_this());
    if (!client) throw std::runtime_error("No client attached to current websocket session. Please reauthenticate.");
    client->refreshToken();
    return {{"token", client->getRawToken()}, {"user", *client->getUser()}};
}

json Auth::logout(Session& session) {
    const auto refreshToken = session.getRefreshToken();
    runtime::Deps::get().authManager->sessionManager()->invalidateSession(session.getUUID());
    session.setAuthenticatedUser(nullptr);
    return {};
}

json Auth::listUsers(const Session& session) {
    if (const auto user = session.getAuthenticatedUser();
        !user->canManageRoles()) throw std::runtime_error("Permission denied: Only admins can list users");
    const auto users = UserQueries::listUsers();
    return {{"users", to_json(users)}};
}

json Auth::isUserAuthenticated(const std::string& token, const Session& session)  {
    const auto client = runtime::Deps::get().authManager->sessionManager()->getClientSession(session.getUUID());
    if (!client) throw std::runtime_error("No access token attached to current websocket session. Please reauthenticate.");

    const auto user = client->getUser();
    if (!user) throw std::runtime_error("User not found for token: " + token);

    const bool isAuthenticated = client->isAuthenticated() && client->validateToken(token);
    json data = {{"isAuthenticated", isAuthenticated}};
    if (isAuthenticated) data["user"] = *user;
    return data;
}

json Auth::getUserByName(const json& payload, const Session& session) {
    const auto user = session.getAuthenticatedUser();
    if (!user) throw std::runtime_error("User not authenticated");

    const auto name = payload.at("name").get<std::string>();
    if (user->name != name && !user->canManageRoles()) throw std::runtime_error(
        "Permission denied: Only admins can fetch user by name");

    const auto retUser = UserQueries::getUserByName(name);
    if (!retUser) throw std::runtime_error("User not found");

    return {{"user", *retUser}};
}

json Auth::doesAdminHaveDefaultPassword() {
    return {{"isDefault", UserQueries::adminPasswordIsDefault()}};
}