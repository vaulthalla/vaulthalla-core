#pragma once

#include "auth/SessionManager.hpp"

#include <string>
#include <unordered_map>
#include <memory>

namespace vh::types {
    class User;
}

namespace vh::auth {

    class AuthManager {
    public:
        AuthManager();

        std::shared_ptr<vh::types::User> registerUser(const std::string& username, const std::string& email, const std::string& password);
        std::shared_ptr<vh::types::User> loginUser(const std::string& email, const std::string& password);
        void changePassword(const std::string& username, const std::string& oldPassword, const std::string& newPassword);
        std::shared_ptr<vh::types::User> findUser(const std::string& email);
        [[nodiscard]] std::shared_ptr<SessionManager> sessionManager() const;
        [[nodiscard]] bool validateToken(const std::string& token);

    private:
        std::unordered_map<std::string, std::shared_ptr<vh::types::User>> users_;
        std::shared_ptr<SessionManager> sessionManager_;

        static bool isValidRegistration(const std::string& name, const std::string& email, const std::string& password);
        static bool isValidName(const std::string& displayName);
        static bool isValidEmail(const std::string& email);
        static bool isValidPassword(const std::string& password);
    };

} // namespace vh::auth
