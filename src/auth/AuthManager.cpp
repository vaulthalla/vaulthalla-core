#include "auth/AuthManager.hpp"
#include "crypto/PasswordHash.hpp"
#include "types/User.hpp"
#include "database/queries/UserQueries.hpp"
#include "crypto/PasswordUtils.hpp"

#include <sodium.h>
#include <stdexcept>
#include <iostream>

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

    std::shared_ptr<vh::types::User> AuthManager::registerUser(const std::string& username, const std::string& email, const std::string& password) {
        if (users_.count(username) > 0) throw std::runtime_error("User already exists: " + username);

        isValidRegistration(username, email, password);
        std::string hashedPassword = vh::crypto::hashPassword(password);
        vh::database::UserQueries::createUser(username, email, hashedPassword);
        auto user = findUser(email);

        std::cout << "[AuthManager] Registered new user: " << username << "\n";
        return user;
    }

    std::shared_ptr<vh::types::User> AuthManager::loginUser(const std::string& email, const std::string& password) {
        auto user = findUser(email);
        if (!user) throw std::runtime_error("User not found: " + email);
        if (!vh::crypto::verifyPassword(password, user->password_hash)) throw std::runtime_error("Invalid password for user: " + email);
        std::cout << "[AuthManager] User logged in: " << email << "\n";
        return user;
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

} // namespace vh::auth
