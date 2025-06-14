#include "auth/AuthManager.hpp"
#include "crypto/PasswordHash.hpp"
#include "types/User.hpp"
#include "database/queries/UserQueries.hpp"
#include "crypto/PasswordUtils.hpp"
#include "auth/SessionManager.hpp"

#include <sodium.h>
#include <stdexcept>
#include <iostream>
#include <jwt-cpp/jwt.h>
#include <chrono>
#include <random>
#include <uuid/uuid.h>

namespace vh::auth {

    AuthManager::AuthManager()
            : sessionManager_(std::make_shared<SessionManager>()) {
        if (sodium_init() < 0)
            throw std::runtime_error("libsodium initialization failed in AuthManager");
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

    std::shared_ptr<SessionManager> AuthManager::sessionManager() const { return sessionManager_; }

    std::shared_ptr<Client> AuthManager::registerUser(const std::string& username,
                                                      const std::string& email,
                                                      const std::string& password,
                                                      const std::string& refreshToken) {
        if (users_.count(username) > 0)
            throw std::runtime_error("User already exists: " + username);

        isValidRegistration(username, email, password);

        const std::string hashedPassword = vh::crypto::hashPassword(password);
        vh::database::UserQueries::createUser(username, email, hashedPassword);

        const auto user = findUser(email);
        auto client = sessionManager_->getClientSession(refreshToken);
        client->setUser(user);

        sessionManager_->promoteSession(client);

        std::cout << "[AuthManager] Registered new user: " << username << "\n";
        return client;
    }

    std::shared_ptr<Client> AuthManager::loginUser(const std::string& email, const std::string& password,
                                                   const std::string& refreshToken) {
        try {
            auto user = findUser(email);
            if (!user) throw std::runtime_error("User not found: " + email);
            if (!vh::crypto::verifyPassword(password, user->password_hash))
                throw std::runtime_error("Invalid password for user: " + email);

            // Optional: Revoke all existing refresh tokens for this user
            vh::database::UserQueries::revokeAllRefreshTokens(user->id);

            // Initialize client (WebSocketSession attached later)
            auto client = sessionManager_->getClientSession(refreshToken);

            // Generate new refresh token (adds to DB + assigns to client)
            createRefreshToken(client);

            sessionManager_->promoteSession(client);

            std::cout << "[AuthManager] User logged in: " << email << "\n";
            return client;
        } catch (const std::exception& e) {
            std::cerr << "[AuthManager] loginUser failed: " << e.what() << "\n";
            return nullptr;
        }
    }

    std::shared_ptr<Client> AuthManager::validateRefreshToken(const std::string& refreshToken,
                                                              const std::shared_ptr<vh::websocket::WebSocketSession>& session) {
        try {
            // 1. Decode and verify JWT
            auto decoded = jwt::decode<jwt::traits::nlohmann_json>(refreshToken);

            const auto verifier = jwt::verify<jwt::traits::nlohmann_json>()
                    .allow_algorithm(jwt::algorithm::hs256{std::getenv("VAULTHALLA_JWT_REFRESH_SECRET")})
                    .with_issuer("") // Optional if you're not setting `iss`
            ;

            verifier.verify(decoded);

            // 2. Extract and validate claims
            const std::string tokenSub = decoded.get_subject();
            const std::string tokenJti = decoded.get_id();
            const auto tokenExp = decoded.get_expires_at();

            if (tokenJti.empty())
                throw std::runtime_error("Missing JTI in refresh token");

            // 3. Lookup refresh token by jti
            auto storedToken = vh::database::UserQueries::getRefreshToken(tokenJti);
            if (!storedToken)
                throw std::runtime_error("Refresh token not found");

            if (storedToken->isRevoked())
                throw std::runtime_error("Refresh token has been revoked");

            auto now = std::chrono::system_clock::now();
            if (std::chrono::system_clock::from_time_t(storedToken->getExpiresAt()) < now)
                throw std::runtime_error("Refresh token has expired");

            // 4. Secure compare stored hash to hashed input token
            if (!vh::crypto::verifyPassword(refreshToken, storedToken->getHashedToken()))
                throw std::runtime_error("Refresh token hash mismatch");

            auto user = vh::database::UserQueries::getUserByRefreshToken(tokenJti);
            auto client = std::make_shared<Client>(session);
            sessionManager_->createSession(client);
            return client;

        } catch (const std::exception& e) {
            std::cerr << "[AuthManager] validateRefreshToken failed: " << e.what() << "\n";
            return nullptr;
        }
    }

    void AuthManager::changePassword(const std::string& email, const std::string& oldPassword, const std::string& newPassword) {
        auto user = findUser(email);
        if (!user) throw std::runtime_error("User not found: " + email);

        if (!vh::crypto::verifyPassword(oldPassword, user->password_hash)) throw std::runtime_error("Invalid old password for user: " + email);

        std::string newHashed = vh::crypto::hashPassword(newPassword);
        user->setPasswordHash(newHashed);

        std::cout << "[AuthManager] Changed password for user: " << email << "\n";
    }

    bool AuthManager::isValidRegistration(const std::string &name, const std::string &email, const std::string &password) {
        std::vector<std::string> errors;

        if (!isValidName(name))
            errors.emplace_back("Name must be between 3 and 50 characters.");

        if (!isValidEmail(email))
            errors.emplace_back("Email must be valid and contain '@' and '.'.");

        unsigned short strength = PasswordUtils::passwordStrengthCheck(password);
        if (strength < 50)
            errors.emplace_back("Password is too weak (strength " + std::to_string(strength) + "/100). Use at least 12 characters, mix upper/lowercase, digits, and symbols.");

        if (PasswordUtils::containsDictionaryWord(password))
            errors.emplace_back("Password contains dictionary word — this is forbidden.");

        if (PasswordUtils::isCommonWeakPassword(password))
            errors.emplace_back("Password matches known weak pattern — this is forbidden.");

        if (PasswordUtils::isPwnedPassword(password))
            errors.emplace_back("Password has been found in public breaches — choose a different one.");

        if (!errors.empty()) {
            std::ostringstream oss;
            oss << "Registration failed due to the following issues:\n";
            for (const auto& err : errors)
                oss << "- " << err << "\n";
            throw std::runtime_error(oss.str());
        }

        return true; // All checks passed
    }

    bool AuthManager::isValidName(const std::string &displayName) {
        return !displayName.empty() && displayName.size() > 2 && displayName.size() <= 50;
    }

    bool AuthManager::isValidEmail(const std::string &email) {
        return !email.empty() && email.find('@') != std::string::npos && email.find('.') != std::string::npos;
    }

    bool AuthManager::isValidPassword(const std::string &password) {
        return !password.empty() && password.size() >= 8 && password.size() <= 128 &&
               std::any_of(password.begin(), password.end(), ::isdigit) && // At least one digit
               std::any_of(password.begin(), password.end(), ::isalpha); // At least one letter
    }

    std::shared_ptr<vh::types::User> AuthManager::findUser(const std::string& email) {
        auto it = users_.find(email);
        if (it != users_.end()) return it->second;

        auto user = vh::database::UserQueries::getUserByEmail(email);
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

    std::string AuthManager::createRefreshToken(const std::shared_ptr<Client>& client) {
        auto now = std::chrono::system_clock::now();
        auto exp = now + std::chrono::hours(24 * 7); // 7 days
        std::string jti = generateUUID();

        std::string token = jwt::create<jwt::traits::nlohmann_json>()
                .set_subject(client->getSession()->getClientIp())
                .set_issued_at(now)
                .set_expires_at(exp)
                .set_id(jti)
                .sign(jwt::algorithm::hs256{std::getenv("VAULTHALLA_JWT_REFRESH_SECRET")});

        auto refreshToken = std::make_shared<RefreshToken>(
            jti,
            vh::crypto::hashPassword(token), // Store hashed token
            0, // User ID will be set later
            client->getSession()->getUserAgent(),
            client->getSession()->getClientIp()
        );

        return token;
    }

} // namespace vh::auth
