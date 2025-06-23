#pragma once

#include "auth/SessionManager.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace vh::types {
class User;
}

namespace vh::websocket {
class WebSocketSession;
}

namespace vh::storage {
class StorageManager;
}

namespace vh::auth {

class Client;

class AuthManager {
public:
    explicit AuthManager(const std::shared_ptr<storage::StorageManager>& storageManager = nullptr);

    void rehydrateOrCreateClient(const std::shared_ptr<websocket::WebSocketSession>& session);

    std::shared_ptr<Client> registerUser(std::shared_ptr<types::User> user,
                                         const std::string& password,
                                         const std::shared_ptr<websocket::WebSocketSession>& session);

    std::shared_ptr<Client> loginUser(const std::string& email, const std::string& password,
                                      const std::shared_ptr<websocket::WebSocketSession>& session);

    void updateUser(const std::shared_ptr<types::User>& user);

    void changePassword(const std::string& username, const std::string& oldPassword, const std::string& newPassword);

    std::shared_ptr<types::User> findUser(const std::string& email);

    [[nodiscard]] std::shared_ptr<SessionManager> sessionManager() const;

    [[nodiscard]] bool validateToken(const std::string& token);

    std::shared_ptr<Client> validateRefreshToken(const std::string& refreshToken,
                                                 const std::shared_ptr<websocket::WebSocketSession>& session);

    static std::pair<std::string, std::shared_ptr<RefreshToken> >
    createRefreshToken(const std::shared_ptr<websocket::WebSocketSession>& session);

private:
    std::unordered_map<std::string, std::shared_ptr<types::User> > users_;
    std::shared_ptr<SessionManager> sessionManager_;
    std::shared_ptr<storage::StorageManager> storageManager_;

    static bool isValidRegistration(const std::shared_ptr<types::User>& user,
                                    const std::string& password);

    static bool isValidName(const std::string& displayName);

    static bool isValidEmail(const std::string& email);

    static bool isValidPassword(const std::string& password);
};

} // namespace vh::auth