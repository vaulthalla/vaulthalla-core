#include "auth/Client.hpp"

#include <chrono>
#include <iostream>
#include <jwt-cpp/jwt.h>
#include <jwt-cpp/traits/nlohmann-json/traits.h>

namespace vh::auth {

Client::Client() = default;

Client::Client(const std::shared_ptr<websocket::WebSocketSession>& session,
               const std::shared_ptr<RefreshToken>& refreshToken, const std::shared_ptr<types::User>& user)
    : user_(user), session_(session), refreshToken_(refreshToken) {
    if (user) {
        token_ = std::make_shared<Token>(generateToken(user->name), user->id);
        session_->setAuthenticatedUser(user_);
    }
}

std::shared_ptr<types::User> Client::getUser() const {
    return user_;
}

std::shared_ptr<Token> Client::getToken() const {
    return token_;
}

std::shared_ptr<websocket::WebSocketSession> Client::getSession() const {
    return session_;
}

void Client::setSession(const std::shared_ptr<websocket::WebSocketSession>& session) {
    if (!session) {
        std::cerr << "[Client] Cannot set session: session is null." << std::endl;
        return;
    }
    session_ = session;
    if (user_) session_->setAuthenticatedUser(user_);
    else std::cerr << "[Client] User is not set, cannot bind to session." << std::endl;
}

void Client::setUser(const std::shared_ptr<types::User>& user) {
    if (!user) {
        std::cerr << "[Client] Cannot set user: user is null." << std::endl;
        return;
    }
    user_ = user;
    token_ = std::make_shared<Token>(generateToken(user->name), user->id);
    session_->setAuthenticatedUser(user_);
}

void Client::setToken(const std::shared_ptr<Token>& token) {
    token_ = token;
}

std::string Client::getUserName() const {
    return user_ ? user_->name : "";
}

std::string Client::getEmail() const {
    return user_ && user_->email ? *user_->email : "";
}

std::string Client::getRawToken() const {
    return token_ ? token_->rawToken : "";
}

bool Client::expiresSoon() const {
    return token_ && token_->expiryTs < std::chrono::system_clock::now() + std::chrono::minutes(5);
}

bool Client::isAuthenticated() const {
    return user_ != nullptr && token_ != nullptr && !token_->isExpired() && !token_->revoked;
}

void Client::refreshToken() {
    if (user_) token_ = std::make_shared<Token>(generateToken(user_->name), user_->id);
    else std::cerr << "[Client] Cannot refresh token: user is not set." << std::endl;
}

void Client::invalidateToken() {
    if (token_) {
        token_->revoke();
        std::cout << "[Client] Token invalidated for user: " << user_->name << std::endl;
    } else {
        std::cerr << "[Client] Cannot invalidate token: token is not set." << std::endl;
    }
}

void Client::closeConnection() {
    if (session_) {
        invalidateToken();
        session_->close();
        std::cout << "[Client] Connection closed for user: " << user_->name << std::endl;
    } else {
        std::cerr << "[Client] Cannot close connection: session is not set." << std::endl;
    }
}

bool Client::validateToken(const std::string& token) const {
    try {
        const auto decoded = jwt::decode<jwt::traits::nlohmann_json>(token);

        const auto verifier = jwt::verify<jwt::traits::nlohmann_json>()
            .allow_algorithm(jwt::algorithm::hs256{jwt_secret_})
            .with_issuer("vaulthalla");

        verifier.verify(decoded);

        return true;
    } catch (const std::exception& e) {
        std::cerr << "[TokenValidator] Token validation failed: " << e.what() << std::endl;
        return false;
    }
}

void Client::sendControlMessage(const std::string& type, const nlohmann::json& payload) const {
    if (!isAuthenticated()) {
        std::cerr << "[Client] Cannot send control message: user is not authenticated." << std::endl;
        return;
    }

    const nlohmann::json msg = {{"type", type}, {"user", user_->email}, {"payload", payload}};

    if (session_) session_->send(msg);
    else std::cerr << "[Client] Cannot send control message: session is not set." << std::endl;
}

std::string Client::generateToken(const std::string& name) const {
    const auto validForMinutes = config::ConfigRegistry::get().auth.token_expiry_minutes;

    return jwt::create<jwt::traits::nlohmann_json>()
        .set_issuer("vaulthalla")
        .set_type("JWS")
        .set_subject(name)
        .set_issued_at(std::chrono::system_clock::now())
        .set_expires_at(std::chrono::system_clock::now() + std::chrono::minutes(validForMinutes))
        .sign(jwt::algorithm::hs256{jwt_secret_});
}

} // namespace vh::auth