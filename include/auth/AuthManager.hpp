#pragma once

#include "auth/SessionManager.hpp"
#include "crypto/InternalSecretManager.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <jwt-cpp/jwt.h>
#include <jwt-cpp/traits/nlohmann-json/traits.h>

namespace vh::identities::model { struct User; }
namespace vh::websocket { class WebSocketSession; }
namespace vh::storage { class Manager; }

namespace vh::auth {

namespace model { class Client; class RefreshToken; }

class AuthManager {
public:
    explicit AuthManager(const std::shared_ptr<storage::Manager>& storageManager = nullptr);

    void rehydrateOrCreateClient(const std::shared_ptr<websocket::WebSocketSession>& session) const;

    std::shared_ptr<model::Client> registerUser(std::shared_ptr<identities::model::User> user,
                                         const std::string& password,
                                         const std::shared_ptr<websocket::WebSocketSession>& session);

    std::shared_ptr<model::Client> loginUser(const std::string& name, const std::string& password,
                                      const std::shared_ptr<websocket::WebSocketSession>& session);

    void updateUser(const std::shared_ptr<identities::model::User>& user);

    void changePassword(const std::string& name, const std::string& oldPassword, const std::string& newPassword);

    std::shared_ptr<identities::model::User> findUser(const std::string& name);

    [[nodiscard]] std::shared_ptr<SessionManager> sessionManager() const;

    void validateRefreshToken(const std::string& refreshToken) const;

    std::shared_ptr<model::Client> validateRefreshToken(const std::string& refreshToken,
                                                 const std::shared_ptr<websocket::WebSocketSession>& session) const;

    std::pair<std::string, std::shared_ptr<model::RefreshToken>>
    createRefreshToken(const std::shared_ptr<websocket::WebSocketSession>& session) const;

    static bool isValidRegistration(const std::shared_ptr<identities::model::User>& user,
                                    const std::string& password);

    static bool isValidName(const std::string& displayName);

    static bool isValidEmail(const std::string& email);

    static bool isValidPassword(const std::string& password);

    static bool isValidGroup(const std::string& group);

private:
    std::unordered_map<std::string, std::shared_ptr<identities::model::User>> users_;
    std::shared_ptr<SessionManager> sessionManager_;
    std::shared_ptr<storage::Manager> storageManager_;
    const std::string jwt_secret_ = crypto::InternalSecretManager().jwtSecret();
};

} // namespace vh::auth