#pragma once

#include "auth/SessionManager.hpp"

#include <string>
#include <unordered_map>
#include <memory>

namespace vh::types {
    class User;
}

namespace vh::websocket {
    class WebSocketSession;
}

namespace vh::auth {

    class Client;

    class AuthManager {
    public:
        AuthManager();

        std::shared_ptr<Client> registerUser(const std::string& username, const std::string& email, const std::string& password,
                                             const std::string& refreshToken);
        std::shared_ptr<Client> loginUser(const std::string& email, const std::string& password,
                                          const std::string& refreshToken);
        void changePassword(const std::string& username, const std::string& oldPassword, const std::string& newPassword);
        std::shared_ptr<vh::types::User> findUser(const std::string& email);
        [[nodiscard]] std::shared_ptr<SessionManager> sessionManager() const;
        [[nodiscard]] bool validateToken(const std::string& token);
        std::shared_ptr<Client> validateRefreshToken(const std::string& refreshToken,
                                                     const std::shared_ptr<vh::websocket::WebSocketSession>& session);

        static std::shared_ptr<RefreshToken> createRefreshToken(const std::shared_ptr<vh::websocket::WebSocketSession>& session);

    private:
        std::unordered_map<std::string, std::shared_ptr<vh::types::User>> users_;
        std::shared_ptr<SessionManager> sessionManager_;

        static bool isValidRegistration(const std::string& name, const std::string& email, const std::string& password);
        static bool isValidName(const std::string& displayName);
        static bool isValidEmail(const std::string& email);
        static bool isValidPassword(const std::string& password);
    };

} // namespace vh::auth
