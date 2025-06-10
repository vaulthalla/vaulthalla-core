#include "auth/AuthManager.hpp"
#include "crypto/PasswordHash.hpp"
#include "types/User.hpp"
#include "database/queries/UserQueries.hpp"

#include <sodium.h>
#include <stdexcept>
#include <iostream>

namespace vh::auth {

    AuthManager::AuthManager()
            : sessionManager_(std::make_shared<SessionManager>()),
              tokenValidator_(std::make_shared<TokenValidator>()) {
        if (sodium_init() < 0)
            throw std::runtime_error("libsodium initialization failed in AuthManager");
    }

    std::shared_ptr<SessionManager> AuthManager::sessionManager() const { return sessionManager_; }

    std::shared_ptr<TokenValidator> AuthManager::tokenValidator() const { return tokenValidator_; }

    std::shared_ptr<vh::types::User> AuthManager::registerUser(const std::string& username, const std::string& email, const std::string& password) {
        if (users_.count(username) > 0) {
            throw std::runtime_error("User already exists: " + username);
        }

        std::string hashedPassword = hashPassword(password);
        vh::database::UserQueries::createUser(username, email, hashedPassword);
        auto user = std::make_shared<vh::types::User>(vh::database::UserQueries::getUserByEmail(email));
        users_[username] = user;

        std::cout << "[AuthManager] Registered new user: " << username << "\n";
        return user;
    }

    std::shared_ptr<vh::types::User> AuthManager::loginUser(const std::string& username, const std::string& password) {
        auto user = findUser(username);
        if (!user) throw std::runtime_error("User not found: " + username);
        if (!verifyPassword(password, user->password_hash)) throw std::runtime_error("Invalid password for user: " + username);
        std::cout << "[AuthManager] User logged in: " << username << "\n";
        return user;
    }

    void AuthManager::changePassword(const std::string& username, const std::string& oldPassword, const std::string& newPassword) {
        auto user = findUser(username);
        if (!user) throw std::runtime_error("User not found: " + username);

        if (!verifyPassword(oldPassword, user->password_hash)) throw std::runtime_error("Invalid old password for user: " + username);

        std::string newHashed = hashPassword(newPassword);
        user->setPasswordHash(newHashed);

        std::cout << "[AuthManager] Changed password for user: " << username << "\n";
    }

    std::shared_ptr<vh::types::User> AuthManager::findUser(const std::string& username) {
        auto it = users_.find(username);
        if (it != users_.end()) return it->second;
        return nullptr;
    }

    // === Internal Helpers ===

    std::string AuthManager::hashPassword(const std::string& password) {
        return vh::crypto::hashPassword(password);
    }

    bool AuthManager::verifyPassword(const std::string& password, const std::string& hash) {
        return vh::crypto::verifyPassword(password, hash);
    }

} // namespace vh::auth
