#pragma once

#include "types/User.hpp"
#include "Token.hpp"
#include "types/RefreshToken.hpp"
#include "protocols/websocket/WebSocketSession.hpp"
#include "config/ConfigRegistry.hpp"
#include <memory>
#include <string>

namespace vh::auth {

class Client {
  public:
    Client();

    Client(const std::shared_ptr<websocket::WebSocketSession>& session,
           const std::shared_ptr<RefreshToken>& refreshToken, const std::shared_ptr<types::User>& user = nullptr);

    [[nodiscard]] std::shared_ptr<types::User> getUser() const;
    [[nodiscard]] std::shared_ptr<Token> getToken() const;
    [[nodiscard]] std::shared_ptr<websocket::WebSocketSession> getSession() const;

    void setUser(const std::shared_ptr<types::User>& user);
    void setToken(const std::shared_ptr<Token>& token);

    [[nodiscard]] std::string getUserName() const;
    [[nodiscard]] std::string getEmail() const;
    [[nodiscard]] std::string getRawToken() const;

    [[nodiscard]] bool expiresSoon() const;
    [[nodiscard]] bool isAuthenticated() const;

    void refreshToken();
    void invalidateToken();
    void closeConnection();

    void setSession(const std::shared_ptr<websocket::WebSocketSession>& session);

    void setRefreshToken(const std::shared_ptr<RefreshToken>& token) { refreshToken_ = token; }
    [[nodiscard]] const std::shared_ptr<RefreshToken>& getRefreshToken() const { return refreshToken_; }
    [[nodiscard]] std::string getHashedRefreshToken() const { return refreshToken_->getHashedToken(); }

    [[nodiscard]] bool validateToken(const std::string& token) const;

    void sendControlMessage(const std::string& type, const nlohmann::json& payload) const;

  private:
    std::shared_ptr<types::User> user_;
    std::shared_ptr<Token> token_{nullptr};
    std::shared_ptr<websocket::WebSocketSession> session_;
    std::shared_ptr<RefreshToken> refreshToken_;
    const std::string jwt_secret_ = config::ConfigRegistry::get().auth.jwt_secret;

    [[nodiscard]] std::string generateToken(const std::string& name) const;
};

} // namespace vh::auth
