#include "auth/AuthManager.hpp"
#include "types/db/User.hpp"
#include "auth/Client.hpp"
#include "auth/SessionManager.hpp"
#include "crypto/PasswordHash.hpp"
#include "crypto/PasswordUtils.hpp"
#include "database/Queries/UserQueries.hpp"
#include "storage/StorageManager.hpp"
#include "websocket/WebSocketSession.hpp"
#include "types/config/ConfigRegistry.hpp"
#include <chrono>
#include <iostream>
#include <jwt-cpp/jwt.h>
#include <sodium.h>
#include <stdexcept>
#include <uuid/uuid.h>
#include <jwt-cpp/traits/nlohmann-json/traits.h>

namespace vh::auth {

AuthManager::AuthManager(const std::shared_ptr<storage::StorageManager>& storageManager)
    : sessionManager_(std::make_shared<SessionManager>()), storageManager_(storageManager) {
    if (sodium_init() < 0) throw std::runtime_error("libsodium initialization failed in AuthManager");
}

void AuthManager::rehydrateOrCreateClient(const std::shared_ptr<websocket::WebSocketSession>& session) {
    std::shared_ptr<Client> client;

    if (!session->getRefreshToken().empty()) {
        std::cout << "[Session] Attempting token rehydration...\n";
        auto validatedClient = validateRefreshToken(session->getRefreshToken(), session);
        if (!validatedClient) std::cout << "[Session] Provided token was invalid or expired.\n";
        else {
            client = validatedClient;
            std::cout << "[Session] Rehydrated session from provided token.\n";
        }
    }

    if (!client) {
        auto tokenPair = createRefreshToken(session);
        client = std::make_shared<Client>(session, tokenPair.second);
        session->setRefreshTokenCookie(tokenPair.first);
    }

    sessionManager_->createSession(client);
}

bool AuthManager::validateToken(const std::string& token) {
    try {
        auto session = sessionManager_->getClientSession(token);

        if (!session) {
            std::cerr << "[AuthManager] Invalid token: session not found for token " << token << "\n";
            return false;
        }

        if (!session->isAuthenticated()) {
            std::cerr << "[AuthManager] Invalid token: session is not authenticated for token " << token << "\n";
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "[AuthManager] Token validation failed: " << e.what() << "\n";
        return false;
    }
}

std::shared_ptr<SessionManager> AuthManager::sessionManager() const {
    return sessionManager_;
}

std::shared_ptr<Client> AuthManager::registerUser(std::shared_ptr<types::User> user,
                                                  const std::string& password,
                                                  const std::shared_ptr<websocket::WebSocketSession>& session) {
    isValidRegistration(user, password);

    user->setPasswordHash(crypto::hashPassword(password));
    database::UserQueries::createUser(user);

    user = findUser(user->email);
    auto client = sessionManager_->getClientSession(session->getUUID());
    client->setUser(user);

    sessionManager_->promoteSession(client);

    storageManager_->initUserStorage(user);

    if (!user) throw std::runtime_error("Failed to create user: " + user->email);

    std::cout << "[AuthManager] Registered new user: " << user->email << "\n";
    return client;
}

std::shared_ptr<Client> AuthManager::loginUser(const std::string& email, const std::string& password,
                                               const std::shared_ptr<websocket::WebSocketSession>& session) {
    try {
        auto user = findUser(email);
        if (!user) throw std::runtime_error("User not found: " + email);

        if (!crypto::verifyPassword(password, user->password_hash))
            throw std::runtime_error(
                "Invalid password for user: " + email);

        database::UserQueries::revokeAllRefreshTokens(user->id);
        database::UserQueries::updateLastLoggedInUser(user->id);
        user = database::UserQueries::getUserById(user->id);
        auto client = sessionManager_->getClientSession(session->getUUID());
        client->setUser(user);
        client->getSession()->setAuthenticatedUser(user);

        sessionManager_->promoteSession(client);

        std::cout << "[AuthManager] User logged in: " << email << "\n";
        return client;
    } catch (const std::exception& e) {
        std::cerr << "[AuthManager] loginUser failed: " << e.what() << "\n";
        return nullptr;
    }
}

void AuthManager::updateUser(const std::shared_ptr<types::User>& user) {
    try {
        if (!user) throw std::runtime_error("Cannot update null user");

        auto existingUser = findUser(user->email);
        if (!existingUser) throw std::runtime_error("User not found: " + user->email);

        existingUser->name = user->name;
        existingUser->email = user->email;
        existingUser->is_active = user->is_active;
        existingUser->role = user->role;

        database::UserQueries::updateUser(existingUser);

        std::cout << "[AuthManager] Updated user: " << user->email << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[AuthManager] updateUser failed: " << e.what() << "\n";
    }
}

std::shared_ptr<Client> AuthManager::validateRefreshToken(const std::string& refreshToken,
                                  const std::shared_ptr<websocket::WebSocketSession>& session) {
    try {
        // 1. Decode and verify JWT
        const auto decoded = jwt::decode<jwt::traits::nlohmann_json>(refreshToken);

        const auto verifier = jwt::verify<jwt::traits::nlohmann_json>()
                .allow_algorithm(jwt::algorithm::hs256{types::config::ConfigRegistry::get().auth.jwt_secret})
                .with_issuer("Vaulthalla") // Optional if you're not setting `iss`
            ;

        verifier.verify(decoded);

        // 2. Extract and validate claims
        const std::string tokenSub = decoded.get_subject();
        const std::string tokenJti = decoded.get_id();
        const auto tokenExp = decoded.get_expires_at();

        if (tokenJti.empty()) throw std::runtime_error("Missing JTI in refresh token");

        // 3. Lookup refresh token by jti
        auto storedToken = database::UserQueries::getRefreshToken(tokenJti);
        if (!storedToken) throw std::runtime_error("Refresh token not found for JTI: " + tokenJti);

        if (storedToken->isRevoked()) throw std::runtime_error("Refresh token has been revoked");

        const auto now = std::chrono::system_clock::now();

        if (std::chrono::system_clock::from_time_t(storedToken->getExpiresAt()) < now)
            throw std::runtime_error(
                "Refresh token has expired");

        // 4. Secure compare stored hash to hashed input token
        if (!crypto::verifyPassword(refreshToken, storedToken->getHashedToken()))
            throw std::runtime_error("Refresh token hash mismatch");

        auto user = database::UserQueries::getUserByRefreshToken(tokenJti);
        auto client = std::make_shared<Client>(session, storedToken, user);
        sessionManager_->createSession(client);
        return client;

    } catch (const std::exception& e) {
        std::cerr << "[AuthManager] validateRefreshToken failed: " << e.what() << "\n";
        return nullptr;
    }
}

void AuthManager::changePassword(const std::string& email, const std::string& oldPassword,
                                 const std::string& newPassword) {
    auto user = findUser(email);
    if (!user) throw std::runtime_error("User not found: " + email);

    if (!crypto::verifyPassword(oldPassword, user->password_hash))
        throw std::runtime_error(
            "Invalid old password for user: " + email);

    std::string newHashed = crypto::hashPassword(newPassword);
    user->setPasswordHash(newHashed);

    std::cout << "[AuthManager] Changed password for user: " << email << "\n";
}

bool AuthManager::isValidRegistration(const std::shared_ptr<types::User>& user, const std::string& password) {
    std::vector<std::string> errors;

    const std::string& name = user->name;
    const std::string& email = user->email;

    if (!isValidName(name)) errors.emplace_back("Name must be between 3 and 50 characters.");

    if (!isValidEmail(email)) errors.emplace_back("Email must be valid and contain '@' and '.'.");

    unsigned short strength = PasswordUtils::passwordStrengthCheck(password);
    if (strength < 50)
        errors.emplace_back("Password is too weak (strength " + std::to_string(strength) +
                            "/100). Use at least 12 characters, mix upper/lowercase, digits, and symbols.");

    if (PasswordUtils::containsDictionaryWord(password))
        errors.emplace_back(
            "Password contains dictionary word — this is forbidden.");

    if (PasswordUtils::isCommonWeakPassword(password))
        errors.emplace_back(
            "Password matches known weak pattern — this is forbidden.");

    if (PasswordUtils::isPwnedPassword(password))
        errors.emplace_back(
            "Password has been found in public breaches — choose a different one.");

    if (!errors.empty()) {
        std::ostringstream oss;
        oss << "Registration failed due to the following issues:\n";
        for (const auto& err : errors) oss << "- " << err << "\n";
        throw std::runtime_error(oss.str());
    }

    return true; // All checks passed
}

bool AuthManager::isValidName(const std::string& displayName) {
    return !displayName.empty() && displayName.size() > 2 && displayName.size() <= 50;
}

bool AuthManager::isValidEmail(const std::string& email) {
    return !email.empty() && email.find('@') != std::string::npos && email.find('.') != std::string::npos;
}

bool AuthManager::isValidPassword(const std::string& password) {
    return !password.empty() && password.size() >= 8 && password.size() <= 128 &&
           std::any_of(password.begin(), password.end(), ::isdigit) && // At least one digit
           std::any_of(password.begin(), password.end(), ::isalpha);   // At least one letter
}

std::shared_ptr<types::User> AuthManager::findUser(const std::string& email) {
    auto it = users_.find(email);
    if (it != users_.end()) return it->second;

    auto user = database::UserQueries::getUserByEmail(email);
    if (user) {
        users_[email] = user;
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

std::pair<std::string, std::shared_ptr<RefreshToken> >
AuthManager::createRefreshToken(const std::shared_ptr<websocket::WebSocketSession>& session) {
    const auto now = std::chrono::system_clock::now();
    const auto exp = now + std::chrono::hours(24 * 7); // 7 days
    std::string jti = generateUUID();

    std::string token =
        jwt::create<jwt::traits::nlohmann_json>()
        .set_issuer("Vaulthalla")
        .set_subject(session->getClientIp() + ":" + session->getUserAgent() + ":" + session->getUUID())
        .set_issued_at(now)
        .set_expires_at(exp)
        .set_id(jti)
        .sign(jwt::algorithm::hs256{types::config::ConfigRegistry::get().auth.jwt_secret});

    return {token,
            std::make_shared<RefreshToken>(jti,
                                           crypto::hashPassword(
                                               token), // Store hashed token
                                           0,          // User ID will be set later
                                           session->getUserAgent(),
                                           session->getClientIp())};
}

} // namespace vh::auth