#include "auth/session/Manager.hpp"
#include "auth/session/Validator.hpp"
#include "auth/session/Resolver.hpp"
#include "protocols/ws/Session.hpp"
#include "auth/model/RefreshToken.hpp"
#include "db/query/identities/User.hpp"
#include "auth/model/TokenPair.hpp"
#include "log/Registry.hpp"

using namespace vh::protocols::ws;

namespace vh::auth::session {

void Manager::newSession(const std::shared_ptr<protocols::ws::Session>& session) {
    if (!session) throw std::invalid_argument("Invalid client or session data");
    if (Validator::hasUsableRefreshToken(session)) Resolver::resolveFromRefreshToken(session);
    ensureSession(session);
}

void Manager::ensureSession(const std::shared_ptr<Session>& session) {
    if (!session) throw std::invalid_argument("Invalid client or session data");

    invalidateSession(std::to_string(session->user->id)); // Invalidate any existing sessions for the user
    cacheSession(session);

    log::Registry::ws()->debug("[SessionManager] Created new unauthenticated session with UUID: {}", session->uuid);
}

void Manager::promoteSession(const std::shared_ptr<Session>& session) {
    if (!session || !Validator::validateSession(session))
        throw std::invalid_argument("Invalid client state for session promotion");

    const auto oldJti = session->tokens->refreshToken->jti;

    session->tokens->refreshToken->userId = session->user->id;

    if (const auto dbToken = db::query::identities::User::getRefreshToken(oldJti)) {
        if (dbToken->userId != session->user->id || dbToken->userAgent != session->userAgent) invalidateSession(dbToken->jti);
    }

    db::query::identities::User::addRefreshToken(session->tokens->refreshToken);

    session->tokens->refreshToken = db::query::identities::User::getRefreshToken(oldJti);
    if (!session->tokens->refreshToken) throw std::runtime_error("Failed to reload refresh token");

    if (!oldJti.empty() && oldJti != session->tokens->refreshToken->jti) invalidateSession(oldJti);

    cacheSession(session);

    log::Registry::ws()->debug("[SessionManager] Promoted session for user: {}", session->user->name);
}

std::shared_ptr<Session> Manager::getSession(const std::string& token) {
    std::lock_guard lock(sessionMutex_);
    if (sessionsByUUID_.contains(token)) return sessionsByUUID_[token];
    if (sessionsByRefreshJti_.contains(token)) return sessionsByRefreshJti_[token];
    if (sessionsByUserId_.contains(std::stoi(token))) return sessionsByUserId_[std::stoi(token)];
    return nullptr;
}

void Manager::cacheSession(const std::shared_ptr<Session>& session) {
    if (!session) throw std::invalid_argument("Invalid session data for caching");

    {
        std::lock_guard lock(sessionMutex_);
        sessionsByRefreshJti_[session->tokens->refreshToken->jti] = session;
        sessionsByUUID_[session->uuid] = session;
        if (session->user) sessionsByUserId_[session->user->id] = session;
    }
}

void Manager::invalidateSession(const std::shared_ptr<protocols::ws::Session>& session) {
    if (!session) throw std::invalid_argument("Invalid session data for invalidation");

    session->tokens->invalidate();

    if (session->user) {
        session->tokens->accessToken->revoke();
        db::query::identities::User::revokeAndPurgeRefreshTokens(session->user->id);
        log::Registry::ws()->debug("[SessionManager] Invalidated session for user: {}", session->user->name);
    }

        {
            std::lock_guard lock(sessionMutex_);
            sessionsByUUID_.erase(session->uuid);
            if (session->tokens->refreshToken) sessionsByRefreshJti_.erase(session->tokens->refreshToken->jti);
            if (session->user) sessionsByUserId_.erase(session->user->id);
        }

    log::Registry::ws()->debug("[SessionManager] Invalidated session with UUID: {}", session->uuid);
}

void Manager::invalidateSession(const std::string& token) {
    const auto session = getSession(token);
    invalidateSession(session);
}

std::unordered_map<std::string, std::shared_ptr<Session> > Manager::getActiveSessions() {
    std::lock_guard lock(sessionMutex_);
    return sessionsByUUID_;
}

}
