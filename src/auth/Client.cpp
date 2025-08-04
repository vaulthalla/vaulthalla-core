#include "auth/Client.hpp"
#include "logging/LogRegistry.hpp"

#include <chrono>
#include <jwt-cpp/jwt.h>
#include <jwt-cpp/traits/nlohmann-json/traits.h>

using namespace vh::auth;
using namespace vh::types;
using namespace vh::websocket;
using namespace vh::logging;
using namespace vh::config;

Client::Client() = default;

Client::Client(const std::shared_ptr<WebSocketSession>& session,
               const std::shared_ptr<RefreshToken>& refreshToken, const std::shared_ptr<User>& user)
    : user_(user), session_(session), refreshToken_(refreshToken) {
    if (user) {
        token_ = std::make_shared<Token>(generateToken(user->name), user->id);
        session_->setAuthenticatedUser(user_);
    }
}

std::shared_ptr<User> Client::getUser() const {
    return user_;
}

std::shared_ptr<Token> Client::getToken() const {
    return token_;
}

std::shared_ptr<WebSocketSession> Client::getSession() const {
    return session_;
}

void Client::setSession(const std::shared_ptr<WebSocketSession>& session) {
    if (!session) {
        LogRegistry::ws()->error("[Client] Cannot set session: session is null.");
        return;
    }
    session_ = session;
    if (user_) session_->setAuthenticatedUser(user_);
    else {
        LogRegistry::ws()->warn("[Client] Session set without user. User must be set before session.");
        session_->setAuthenticatedUser(nullptr);
    }
}

void Client::setUser(const std::shared_ptr<User>& user) {
    if (!user) {
        LogRegistry::ws()->error("[Client] Cannot set user: user is null.");
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
    else LogRegistry::ws()->error("[Client] Cannot refresh token: user is not set.");
}

void Client::invalidateToken() {
    if (token_) {
        token_->revoke();
        LogRegistry::ws()->info("[Client] Token invalidated for user: {}", user_ ? user_->name : "unknown");
    } else LogRegistry::ws()->error("[Client] Cannot invalidate token: token is not set.");
}

void Client::closeConnection() {
    if (session_) {
        invalidateToken();
        session_->close();
        LogRegistry::ws()->info("[Client] Connection closed for user: {}", user_ ? user_->name : "unknown");
    } else LogRegistry::ws()->error("[Client] Cannot close connection: session is not set.");
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
        LogRegistry::ws()->error("[Client] Token validation failed: {}", e.what());
        return false;
    }
}

void Client::sendControlMessage(const std::string& type, const nlohmann::json& payload) const {
    if (!isAuthenticated()) {
        LogRegistry::ws()->error("[Client] Cannot send control message: client is not authenticated.");
        return;
    }

    const nlohmann::json msg = {{"type", type}, {"user", user_->email}, {"payload", payload}};

    if (session_) session_->send(msg);
    else LogRegistry::ws()->error("[Client] Cannot send control message: session is not set.");
}

std::string Client::generateToken(const std::string& name) const {
    const auto validForMinutes = ConfigRegistry::get().auth.token_expiry_minutes;

    return jwt::create<jwt::traits::nlohmann_json>()
        .set_issuer("vaulthalla")
        .set_type("JWS")
        .set_subject(name)
        .set_issued_at(std::chrono::system_clock::now())
        .set_expires_at(std::chrono::system_clock::now() + std::chrono::minutes(validForMinutes))
        .sign(jwt::algorithm::hs256{jwt_secret_});
}
