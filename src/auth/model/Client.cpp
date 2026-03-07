#include "auth/model/Client.hpp"
#include "log/Registry.hpp"
#include "config/Registry.hpp"

#include <chrono>
#include <jwt-cpp/jwt.h>
#include <jwt-cpp/traits/nlohmann-json/traits.h>

using namespace vh::auth;
using namespace vh::auth::model;
using namespace vh::protocols;
using namespace vh::config;
using namespace vh::identities::model;

Client::Client() = default;

Client::Client(const std::shared_ptr<ws::Session>& session,
               const std::shared_ptr<RefreshToken>& refreshToken, const std::shared_ptr<User>& user)
    : user(user), session(session), refreshToken(refreshToken) {
    if (user) {
        token = std::make_shared<Token>(generateToken(user->name), user->id);
        session->setAuthenticatedUser(user);
    }
}

void Client::setSession(const std::shared_ptr<ws::Session>& session) {
    if (!session) {
        log::Registry::ws()->debug("[Client] Cannot set session: session is null.");
        return;
    }
    this->session = session;
    if (user) session->setAuthenticatedUser(user);
    else {
        log::Registry::ws()->debug("[Client] Session set without user. User must be set before session.");
        session->setAuthenticatedUser(nullptr);
    }
}

void Client::setUser(const std::shared_ptr<User>& user) {
    if (!user) {
        log::Registry::ws()->debug("[Client] Cannot set user: user is null.");
        return;
    }
    this->user = user;
    renewToken();
    session->setAuthenticatedUser(user);
}

void Client::setToken(const std::shared_ptr<Token>& token) { this->token = token; }

bool Client::expiresSoon() const {
    return token && token->expiryTs < std::chrono::system_clock::now() + std::chrono::minutes(5);
}

bool Client::isAuthenticated() const {
    return user && token && token->isValid();
}

void Client::renewToken() {
    if (user) token = std::make_shared<Token>(generateToken(user->name), user->id);
    else log::Registry::ws()->debug("[Client] Cannot refresh token: user is not set.");
}

void Client::invalidateToken() const {
    if (token) {
        token->revoke();
        log::Registry::ws()->debug("[Client] Token invalidated for user: {}", user ? user->name : "unknown");
    } else log::Registry::ws()->debug("[Client] Cannot invalidate token: token is not set.");
}

void Client::closeConnection() const {
    invalidateToken();
    if (session) session->close();
    log::Registry::ws()->debug("[Client] Connection closed for user: {}", user ? user->name : "unknown");
}

std::string Client::getRawToken() const { return token ? token->rawToken : std::string(); }

bool Client::validateSession() const {
    return user && token && token->isValid() && refreshToken && refreshToken->isValid();
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
        log::Registry::ws()->debug("[Client] Token validation failed: {}", e.what());
        return false;
    }
}

void Client::sendControlMessage(const std::string& type, const nlohmann::json& payload) const {
    if (!isAuthenticated()) {
        log::Registry::ws()->debug("[Client] Cannot send control message: client is not authenticated.");
        return;
    }

    const nlohmann::json msg = {{"type", type}, {"user", user->email}, {"payload", payload}};

    if (session) session->send(msg);
    else log::Registry::ws()->debug("[Client] Cannot send control message: session is not set.");
}

std::string Client::generateToken(const std::string& name) const {
    const auto validForMinutes = Registry::get().auth.token_expiry_minutes;

    return jwt::create<jwt::traits::nlohmann_json>()
        .set_issuer("vaulthalla")
        .set_type("JWS")
        .set_subject(name)
        .set_issued_at(std::chrono::system_clock::now())
        .set_expires_at(std::chrono::system_clock::now() + std::chrono::minutes(validForMinutes))
        .sign(jwt::algorithm::hs256{jwt_secret_});
}
