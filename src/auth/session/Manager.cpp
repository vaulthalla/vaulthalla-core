#include "auth/session/Manager.hpp"

#include "auth/model/RefreshToken.hpp"
#include "auth/model/TokenPair.hpp"
#include "auth/session/Resolver.hpp"
#include "auth/session/Validator.hpp"
#include "db/query/auth/RefreshToken.hpp"
#include "log/Registry.hpp"
#include "protocols/ws/Session.hpp"

using namespace vh::protocols::ws;
using namespace vh::identities::model;

namespace vh::auth::session {

void Manager::tryRehydrateSession(const SessionPtr& session) {
    if (!session) throw std::invalid_argument("Invalid session");

    if (Validator::hasUsableRefreshToken(session))
        Resolver::resolveFromRefreshToken(session);

    ensureSession(session);
}

void Manager::ensureSession(const SessionPtr& session) {
    if (!session) throw std::invalid_argument("Invalid session");

    cacheSession(session);

    log::Registry::ws()->debug(
        "[SessionManager] Ensured session with UUID: {}",
        session->uuid
    );
}

void Manager::promoteSession(const SessionPtr& session) {
    if (!session || !Validator::validateSession(session))
        throw std::invalid_argument("Invalid session state for promotion");

    const auto oldJti = session->tokens->refreshToken->jti;

    if (const auto dbToken = db::query::auth::RefreshToken::getRefreshToken(oldJti);
        dbToken->dangerousDivergence(session->tokens->refreshToken))
            invalidateSession(oldJti);

    db::query::auth::RefreshToken::setRefreshToken(session->tokens->refreshToken);

    session->tokens->refreshToken = db::query::auth::RefreshToken::getRefreshToken(oldJti);
    if (!session->tokens->refreshToken)
        throw std::runtime_error("Failed to reload refresh token");

    if (!oldJti.empty() && oldJti != session->tokens->refreshToken->jti)
        invalidateSession(oldJti);

    cacheSession(session);

    log::Registry::ws()->debug(
        "[SessionManager] Promoted session for user: {}",
        session->user->name
    );
}

void Manager::cacheSession(const SessionPtr& session) {
    if (!session) throw std::invalid_argument("Invalid session data for caching");

    std::lock_guard lock(sessionMutex_);

    sessionsByUUID_[session->uuid] = session;

    if (session->tokens && session->tokens->refreshToken && !session->tokens->refreshToken->jti.empty())
        sessionsByRefreshJti_[session->tokens->refreshToken->jti] = session;

    if (session->user)
        sessionsByUserId_.emplace(session->user->id, session);
}

void Manager::invalidateSession(const SessionPtr& session) {
    if (!session) return;

    const auto uuid = session->uuid;
    const auto jti =
        (session->tokens && session->tokens->refreshToken)
            ? session->tokens->refreshToken->jti
            : std::string{};
    const auto userId =
        session->user ? std::optional<uint32_t>{session->user->id} : std::nullopt;

    if (session->tokens)
        session->tokens->invalidate();

    {
        std::lock_guard lock(sessionMutex_);

        sessionsByUUID_.erase(uuid);

        if (!jti.empty())
            sessionsByRefreshJti_.erase(jti);

        if (userId) {
            auto [begin, end] = sessionsByUserId_.equal_range(*userId);
            for (auto it = begin; it != end;) {
                if (it->second == session) it = sessionsByUserId_.erase(it);
                else ++it;
            }
        }
    }

    log::Registry::ws()->debug(
        "[SessionManager] Invalidated session with UUID: {}",
        uuid
    );
}

void Manager::invalidateSession(const std::string& token) {
    invalidateSession(getSession(token));
}

Manager::SessionPtr Manager::getSession(const std::string& token) {
    std::lock_guard lock(sessionMutex_);

    if (const auto it = sessionsByUUID_.find(token); it != sessionsByUUID_.end())
        return it->second;

    if (const auto it = sessionsByRefreshJti_.find(token); it != sessionsByRefreshJti_.end())
        return it->second;

    return nullptr;
}

std::vector<Manager::SessionPtr> Manager::getSessions(const std::shared_ptr<User>& user) {
    if (!user) throw std::invalid_argument("Invalid user");
    return getSessionsByUserId(user->id);
}

std::vector<Manager::SessionPtr> Manager::getSessionsByUserId(const uint32_t userId) {
    std::lock_guard lock(sessionMutex_);

    std::vector<SessionPtr> sessions;
    const auto [begin, end] = sessionsByUserId_.equal_range(userId);

    for (auto it = begin; it != end; ++it)
        sessions.push_back(it->second);

    return sessions;
}

Manager::SessionMap Manager::getActiveSessions() {
    std::lock_guard lock(sessionMutex_);
    return sessionsByUUID_;
}

}
