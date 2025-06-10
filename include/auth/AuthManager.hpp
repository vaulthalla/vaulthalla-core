#pragma once

#include "auth/SessionManager.hpp"
#include "auth/TokenValidator.hpp"

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
        std::shared_ptr<vh::types::User> loginUser(const std::string& username, const std::string& password);
        void changePassword(const std::string& username, const std::string& oldPassword, const std::string& newPassword);

        // Simplified user storage for now (later: persist to DB)
        std::shared_ptr<vh::types::User> findUser(const std::string& username);

        [[nodiscard]] std::shared_ptr<SessionManager> sessionManager() const;
        [[nodiscard]] std::shared_ptr<TokenValidator> tokenValidator() const;

    private:
        std::unordered_map<std::string, std::shared_ptr<vh::types::User>> users_;
        std::shared_ptr<SessionManager> sessionManager_;
        std::shared_ptr<TokenValidator> tokenValidator_;

        static std::string hashPassword(const std::string& password);
        static bool verifyPassword(const std::string& password, const std::string& hash);

        // Volume key helpers
        std::pair<std::string, std::string> generateVolumeKeyPair();
        std::string encryptPrivateKey(const std::string& privateKey, const std::string& password);
        std::string decryptPrivateKey(const std::string& encryptedPrivateKey, const std::string& password);
    };

} // namespace vh::auth
