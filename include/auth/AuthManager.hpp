#pragma once

#include "auth/User.hpp"
#include "auth/SessionManager.hpp"
#include "auth/TokenValidator.hpp"

#include <string>
#include <unordered_map>
#include <memory>

namespace vh::auth {

    class AuthManager {
    public:
        AuthManager(SessionManager& sessionManager, TokenValidator& tokenValidator);

        std::shared_ptr<User> registerUser(const std::string& username, const std::string& password);
        std::shared_ptr<User> loginUser(const std::string& username, const std::string& password);
        void changePassword(const std::string& username, const std::string& oldPassword, const std::string& newPassword);

        // Simplified user storage for now (later: persist to DB)
        std::shared_ptr<User> findUser(const std::string& username);

    private:
        std::unordered_map<std::string, std::shared_ptr<User>> users_;
        SessionManager& sessionManager_;
        TokenValidator& tokenValidator_;

        std::string hashPassword(const std::string& password);
        bool verifyPassword(const std::string& password, const std::string& hash);

        // Volume key helpers
        std::pair<std::string, std::string> generateVolumeKeyPair();
        std::string encryptPrivateKey(const std::string& privateKey, const std::string& password);
        std::string decryptPrivateKey(const std::string& encryptedPrivateKey, const std::string& password);
    };

} // namespace vh::auth
