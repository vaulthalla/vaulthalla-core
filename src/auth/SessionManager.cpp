#include "auth/SessionManager.hpp"
#include "auth/model/RefreshToken.hpp"
#include "db/query/identities/User.hpp"
#include "log/Registry.hpp"

using namespace vh::auth;
using namespace vh::auth::model;

void SessionManager::createSession(const std::shared_ptr<Client>& client) {
    std::lock_guard lock(sessionMutex_);

    if (!client || !client->session) throw std::invalid_argument("Invalid client or session data");

    sessionsByRefreshJti_[client->refreshToken->getJti()] = client;
    sessionsByUUID_[client->session->getUUID()] = client;
    if (const auto user = client->user) sessionsByUserId_[user->id] = client; // unlikely but just in case

    log::Registry::ws()->debug("[SessionManager] Created new unauthenticated session with UUID: {}", client->session->getUUID());
}

std::string SessionManager::promoteSession(const std::shared_ptr<Client>& client) {
    if (!client || !client->validateSession())
        throw std::invalid_argument("Invalid client state for session promotion");

    const auto oldJti = client->refreshToken->getJti();
    const auto userId = client->user->id;
    const auto userAgent = client->session->getUserAgent();
    const auto ipAddress = client->session->getClientIp();

    const auto token = client->refreshToken;
    token->setUserId(userId);
    token->setUserAgent(userAgent);
    token->setIpAddress(ipAddress);

    if (const auto dbToken = db::query::identities::User::getRefreshToken(oldJti);
        dbToken->getUserId() != userId || dbToken->getUserAgent() != userAgent)
            throw std::invalid_argument("Invalid refresh token");

    db::query::identities::User::addRefreshToken(token);

    client->setRefreshToken(db::query::identities::User::getRefreshToken(oldJti));
    if (!client->refreshToken) throw std::runtime_error("Failed to reload refresh token");

    {
        std::lock_guard lock(sessionMutex_);

        const auto newJti = client->refreshToken->getJti();

        if (!oldJti.empty() && oldJti != newJti)
            sessionsByRefreshJti_.erase(oldJti);

        sessionsByRefreshJti_[newJti] = client;
        sessionsByUUID_[client->session->getUUID()] = client;
        sessionsByUserId_[userId] = client;
    }

    log::Registry::ws()->debug("[SessionManager] Promoted session for user: {}", client->user->name);
    return client->getRawToken();
}

std::shared_ptr<Client> SessionManager::getClientSession(const std::string& token) {
    std::lock_guard lock(sessionMutex_);
    if (sessionsByUUID_.contains(token)) return sessionsByUUID_[token];
    if (sessionsByRefreshJti_.contains(token)) return sessionsByRefreshJti_[token];
    if (sessionsByUserId_.contains(std::stoi(token))) return sessionsByUserId_[std::stoi(token)];
    return nullptr;
}

void SessionManager::invalidateSession(const std::string& token) {
    const auto client = getClientSession(token);

    if (!client || !client->session) {
        log::Registry::ws()->warn("[SessionManager] Session not found for token: {}", token);
        return;
    }

    const auto user = client->user;

    {
        std::lock_guard lock(sessionMutex_);
        sessionsByUUID_.erase(client->session->getUUID());
        sessionsByRefreshJti_.erase(client->refreshToken->getJti());
        if (user) sessionsByUserId_.erase(user->id);
    }

    if (user) {
        client->invalidateToken();
        db::query::identities::User::revokeAndPurgeRefreshTokens(user->id);
        log::Registry::ws()->debug("[SessionManager] Invalidated session: {}", token);
    }
}

std::unordered_map<std::string, std::shared_ptr<Client> > SessionManager::getActiveSessions() {
    std::lock_guard lock(sessionMutex_);
    return sessionsByUUID_;
}
