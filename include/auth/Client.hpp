#pragma once

#include "../../shared/include/types/db/User.hpp"
#include "Token.hpp"
#include "websocket/WebSocketSession.hpp"
#include "auth/RefreshToken.hpp"

#include <sodium.h>
#include <jwt-cpp/jwt.h>
#include <jwt-cpp/traits/nlohmann-json/traits.h>
#include <iostream>
#include <memory>
#include <string>
#include <chrono>

namespace vh::auth {

    class Client {
    public:
        Client();

        Client(const std::shared_ptr<vh::websocket::WebSocketSession>& session,
               const std::shared_ptr<RefreshToken>& refreshToken,
               const std::shared_ptr<vh::types::User>& user = nullptr);

        [[nodiscard]] std::shared_ptr<vh::types::User> getUser() const;
        [[nodiscard]] std::shared_ptr<Token> getToken() const;
        [[nodiscard]] std::shared_ptr<vh::websocket::WebSocketSession> getSession() const;

        void setUser(const std::shared_ptr<vh::types::User>& user);
        void setToken(const std::shared_ptr<Token>& token);

        [[nodiscard]] std::string getUserName() const;
        [[nodiscard]] std::string getEmail() const;
        [[nodiscard]] std::string getRawToken() const;

        [[nodiscard]] bool expiresSoon() const;
        [[nodiscard]] bool isAuthenticated() const;

        void refreshToken();
        void invalidateToken();
        void closeConnection();

        void setRefreshToken(const std::shared_ptr<RefreshToken>& token) { refreshToken_ = token; }
        [[nodiscard]] const std::shared_ptr<RefreshToken>& getRefreshToken() const { return refreshToken_; }
        [[nodiscard]] std::string getHashedRefreshToken() const { return refreshToken_->getHashedToken(); }

        bool validateToken(const std::string& token) const;

        void sendControlMessage(const std::string& type, const nlohmann::json& payload);

    private:
        std::shared_ptr<vh::types::User> user_;
        std::shared_ptr<Token> token_;
        std::shared_ptr<vh::websocket::WebSocketSession> session_;
        std::shared_ptr<RefreshToken> refreshToken_;
        const std::string jwt_secret_ = std::getenv("VAULTHALLA_JWT_SECRET");

        std::string generateToken(const std::string& email);
    };

} // namespace vh::auth
