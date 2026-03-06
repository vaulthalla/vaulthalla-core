#pragma once

#include "identities/model/User.hpp"
#include "Token.hpp"
#include "RefreshToken.hpp"
#include "protocols/ws/Session.hpp"
#include "crypto/secrets/Manager.hpp"
#include <memory>
#include <string>

namespace vh::auth::model {

class Client {
  public:
    std::shared_ptr<identities::model::User> user{nullptr};
    std::shared_ptr<protocols::ws::Session> session{nullptr};
    std::shared_ptr<Token> token{nullptr};
    std::shared_ptr<RefreshToken> refreshToken{nullptr};

    Client();

    Client(const std::shared_ptr<protocols::ws::Session>& session,
           const std::shared_ptr<RefreshToken>& refreshToken, const std::shared_ptr<identities::model::User>& user = nullptr);

    void setUser(const std::shared_ptr<identities::model::User>& user);
    void setToken(const std::shared_ptr<Token>& token);

    [[nodiscard]] bool validateSession() const;
    [[nodiscard]] bool validateToken(const std::string& token) const;
    void renewToken();
    [[nodiscard]] std::string getRawToken() const;

    [[nodiscard]] bool expiresSoon() const;
    [[nodiscard]] bool isAuthenticated() const;

    void invalidateToken() const;
    void closeConnection() const;

    void setSession(const std::shared_ptr<protocols::ws::Session>& session);

    void setRefreshToken(const std::shared_ptr<RefreshToken>& token) { refreshToken = token; }
    [[nodiscard]] std::string getHashedRefreshToken() const { return refreshToken->getHashedToken(); }

    void sendControlMessage(const std::string& type, const nlohmann::json& payload) const;

    [[nodiscard]] std::chrono::system_clock::time_point connOpenedAt() const { return openedAt_; }

  private:

    const std::string jwt_secret_ = crypto::secrets::Manager().jwtSecret();
    std::chrono::system_clock::time_point openedAt_ = std::chrono::system_clock::now();

    [[nodiscard]] std::string generateToken(const std::string& name) const;
};

}
