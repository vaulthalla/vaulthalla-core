#include "auth/AuthManager.hpp"
#include "identities/model/User.hpp"
#include "auth/model/Client.hpp"
#include "auth/SessionManager.hpp"
#include "crypto/util/hash.hpp"
#include "crypto/password/Strength.hpp"
#include "database/Queries/UserQueries.hpp"
#include "storage/Manager.hpp"
#include "protocols/ws/Session.hpp"
#include "config/ConfigRegistry.hpp"
#include "logging/LogRegistry.hpp"

#include <chrono>
#include <sodium.h>
#include <stdexcept>
#include <uuid/uuid.h>
#include <ranges>
#include <paths.h>

using namespace vh::auth;
using namespace vh::auth::model;
using namespace vh::identities::model;
using namespace vh::crypto;
using namespace vh::database;
using namespace vh::storage;
using namespace vh::logging;
using namespace vh::protocols;

AuthManager::AuthManager(const std::shared_ptr<Manager>& storageManager)
    : sessionManager_(std::make_shared<SessionManager>()), storageManager_(storageManager) {
    if (sodium_init() < 0) throw std::runtime_error("libsodium initialization failed in AuthManager");
}

void AuthManager::rehydrateOrCreateClient(const std::shared_ptr<ws::Session>& session) const {
    std::shared_ptr<Client> client;

    if (!session->getRefreshToken().empty()) {
        LogRegistry::auth()->debug("[AuthManager] Attempting to rehydrate session from provided refresh token.");
        const auto validatedClient = validateRefreshToken(session->getRefreshToken(), session);
        if (!validatedClient) LogRegistry::auth()->debug("[AuthManager] Failed to rehydrate session: invalid refresh token.");
        else {
            client = validatedClient;
            LogRegistry::auth()->debug("[AuthManager] Successfully rehydrated session for user: {}", client->getUserName());
        }
    }

    if (!client) {
        const auto tokenPair = createRefreshToken(session);
        client = std::make_shared<Client>(session, tokenPair.second);
        session->setRefreshTokenCookie(tokenPair.first);
    }

    sessionManager_->createSession(client);
}

std::shared_ptr<SessionManager> AuthManager::sessionManager() const {
    return sessionManager_;
}

std::shared_ptr<Client> AuthManager::registerUser(std::shared_ptr<User> user,
                                                  const std::string& password,
                                                  const std::shared_ptr<ws::Session>& session) {
    isValidRegistration(user, password);

    user->setPasswordHash(hash::password(password));
    UserQueries::createUser(user);

    user = findUser(user->name);
    auto client = sessionManager_->getClientSession(session->getUUID());
    client->setUser(user);

    sessionManager_->promoteSession(client);
    storageManager_->initUserStorage(user);

    if (!user) throw std::runtime_error("Failed to create user: " + user->name);

    LogRegistry::auth()->info("[AuthManager] User registered: {}", user->name);
    return client;
}

std::shared_ptr<Client> AuthManager::loginUser(const std::string& name, const std::string& password,
                                               const std::shared_ptr<ws::Session>& session) {
    try {
        auto user = findUser(name);
        if (!user) throw std::runtime_error("User not found: " + name);

        if (!hash::verifyPassword(password, user->password_hash))
            throw std::runtime_error("Invalid password for user: " + name);

        UserQueries::revokeAllRefreshTokens(user->id);
        UserQueries::updateLastLoggedInUser(user->id);
        user = UserQueries::getUserById(user->id);
        auto client = sessionManager_->getClientSession(session->getUUID());
        client->setUser(user);
        client->getSession()->setAuthenticatedUser(user);

        sessionManager_->promoteSession(client);

        LogRegistry::auth()->info("[AuthManager] User logged in: {}", user->name);
        return client;
    } catch (const std::exception& e) {
        LogRegistry::auth()->error("[AuthManager] loginUser failed: {}", e.what());
        return nullptr;
    }
}

void AuthManager::updateUser(const std::shared_ptr<User>& user) {
    try {
        if (!user) throw std::runtime_error("Cannot update null user");

        UserQueries::updateUser(user);
        users_[user->name] = UserQueries::getUserById(user->id);

        LogRegistry::auth()->debug("[AuthManager] User updated: {}", user->name);
    } catch (const std::exception& e) {
        LogRegistry::auth()->error("[AuthManager] updateUser failed: {}", e.what());
        throw std::runtime_error("Failed to update user: " + user->name + " - " + e.what());
    }
}

void AuthManager::validateRefreshToken(const std::string& refreshToken) const {
    const auto decoded = jwt::decode<jwt::traits::nlohmann_json>(refreshToken);
    const std::string tokenJti = decoded.get_id();

    if (const auto session = sessionManager_->getClientSession(tokenJti)) {
        if (!session->isAuthenticated()) throw std::runtime_error("Prior session is not authenticated");
        return;
    }

    throw std::runtime_error("HTTP validation requires an existing websocket authenticated session. "
                             "Please authenticate via WebSocket first. "
                             "Session not found for token JTI: " + tokenJti);
}

std::shared_ptr<Client> AuthManager::validateRefreshToken(const std::string& refreshToken,
                                  const std::shared_ptr<ws::Session>& session) const {
    try {
        // 1. Decode and verify JWT
        const auto decoded = jwt::decode<jwt::traits::nlohmann_json>(refreshToken);
        const std::string tokenJti = decoded.get_id();

        if (const auto priorSession = sessionManager_->getClientSession(tokenJti)) {
            if (!priorSession->isAuthenticated()) throw std::runtime_error("Prior session is not authenticated");
            LogRegistry::auth()->debug("[AuthManager] Rehydrating existing session for JTI: {}", tokenJti);
            priorSession->setSession(session);
            return priorSession; // Session already exists, rehydrate it
        }

        const auto verifier = jwt::verify<jwt::traits::nlohmann_json>()
                .allow_algorithm(jwt::algorithm::hs256{jwt_secret_})
                .with_issuer("Vaulthalla")
        ;

        verifier.verify(decoded);

        // 2. Extract and validate claims
        const std::string tokenSub = decoded.get_subject();
        const auto tokenExp = decoded.get_expires_at();

        if (tokenExp < std::chrono::system_clock::now()) throw std::runtime_error("Refresh token has expired");

        if (tokenJti.empty()) throw std::runtime_error("Missing JTI in refresh token");

        // 3. Lookup refresh token by jti
        auto storedToken = UserQueries::getRefreshToken(tokenJti);
        if (!storedToken) {
            LogRegistry::auth()->debug("[AuthManager] Refresh token not found: {}", tokenJti);
            return nullptr;
        }

        if (storedToken->isRevoked()) throw std::runtime_error("Refresh token has been revoked");

        if (std::chrono::system_clock::from_time_t(storedToken->getExpiresAt()) < std::chrono::system_clock::now())
            throw std::runtime_error("Refresh token has expired");

        // 4. Secure compare stored hash to hashed input token
        if (!hash::verifyPassword(refreshToken, storedToken->getHashedToken()))
            throw std::runtime_error("Refresh token hash mismatch");

        auto user = UserQueries::getUserByRefreshToken(tokenJti);
        if (!user) {
            LogRegistry::auth()->debug("[AuthManager] User not found for refresh token: {}", tokenJti);
            return nullptr;
        }

        auto client = std::make_shared<Client>(session, storedToken, user);
        sessionManager_->createSession(client);

        return client;
    } catch (const std::exception& e) {
        // making this debug to avoid flooding logs with errors
        LogRegistry::auth()->debug("[AuthManager] validateRefreshToken failed: {}", e.what());
        return nullptr;
    }
}

void AuthManager::changePassword(const std::string& name, const std::string& oldPassword,
                                 const std::string& newPassword) {
    const auto user = findUser(name);
    if (!user) throw std::runtime_error("User not found: " + name);

    if (!hash::verifyPassword(oldPassword, user->password_hash))
        throw std::runtime_error("Invalid old password for user: " + name);

    user->setPasswordHash(hash::password(newPassword));

    LogRegistry::audit()->info("[AuthManager] User {} is changing password", user->name);
    LogRegistry::auth()->info("[AuthManager] Changing password for user: {}", user->name);
}

bool AuthManager::isValidRegistration(const std::shared_ptr<User>& user, const std::string& password) {
    std::vector<std::string> errors;

    if (!isValidName(user->name)) errors.emplace_back("Name must be between 3 and 50 characters.");

    if (user->email && !isValidEmail(*user->email)) errors.emplace_back("Email must be valid and contain '@' and '.'.");

    if (!paths::testMode) {
        if (const auto strength = password::Strength::passwordStrengthCheck(password) < 50)
            errors.emplace_back("Password is too weak (strength " + std::to_string(strength) +
                                "/100). Use at least 12 characters, mix upper/lowercase, digits, and symbols.");

        if (password::Strength::containsDictionaryWord(password))
            errors.emplace_back("Password contains dictionary word — this is forbidden.");

        if (password::Strength::isCommonWeakPassword(password))
            errors.emplace_back("Password matches known weak pattern — this is forbidden.");

        if (password::Strength::isPwnedPassword(password))
            errors.emplace_back("Password has been found in public breaches — choose a different one.");
    }

    if (!errors.empty()) {
        std::ostringstream oss;
        oss << "Registration failed due to the following issues:\n";
        for (const auto& err : errors) oss << "- " << err << std::endl;
        LogRegistry::auth()->error("[AuthManager] Registration validation failed: {}", oss.str());
        throw std::runtime_error(oss.str());
    }

    return true;
}

bool AuthManager::isValidName(const std::string& displayName) {
    return !displayName.empty() && displayName.size() > 2 && displayName.size() <= 50;
}

bool AuthManager::isValidEmail(const std::string& email) {
    return !email.empty() && email.find('@') != std::string::npos && email.find('.') != std::string::npos;
}

bool AuthManager::isValidPassword(const std::string& password) {
    if (paths::testMode) return true;
    std::vector<std::string> errors;
    if (password::Strength::passwordStrengthCheck(password) < 50) return false;
    if (password::Strength::containsDictionaryWord(password)) return false;
    if (password::Strength::isCommonWeakPassword(password)) return false;
    if (password::Strength::isPwnedPassword(password)) return false;
    return !password.empty() && password.size() >= 8 && password.size() <= 128 &&
           std::ranges::any_of(password.begin(), password.end(), ::isdigit) && // At least one digit
           std::ranges::any_of(password.begin(), password.end(), ::isalpha);   // At least one letter
}

bool AuthManager::isValidGroup(const std::string& group) {
    return !group.empty() && group.size() >= 3 && group.size() <= 50;
}

std::shared_ptr<User> AuthManager::findUser(const std::string& name) {
    const auto it = users_.find(name);
    if (it != users_.end()) return it->second;

    if (const auto user = UserQueries::getUserByName(name)) {
        users_[name] = user;
        return user;
    }
    return nullptr;
}

std::string generateUUID() {
    uuid_t uuid;
    char uuidStr[37];
    uuid_generate(uuid);
    uuid_unparse(uuid, uuidStr);
    return {uuidStr};
}

std::pair<std::string, std::shared_ptr<RefreshToken>>
AuthManager::createRefreshToken(const std::shared_ptr<ws::Session>& session) const {
    const auto now = std::chrono::system_clock::now();
    const auto exp = now + std::chrono::days(config::ConfigRegistry::get().auth.refresh_token_expiry_days);
    std::string jti = generateUUID();

    std::string token =
        jwt::create<jwt::traits::nlohmann_json>()
        .set_issuer("Vaulthalla")
        .set_subject(session->getClientIp() + ":" + session->getUserAgent() + ":" + session->getUUID())
        .set_issued_at(now)
        .set_expires_at(exp)
        .set_id(jti)
        .sign(jwt::algorithm::hs256{jwt_secret_});

    return {token,
            std::make_shared<RefreshToken>(jti,
                                           hash::password(token),     // Store hashed token
                                           0,                       // User ID will be set later
                                           session->getUserAgent(),
                                           session->getClientIp())};
}
