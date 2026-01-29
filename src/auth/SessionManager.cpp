#include "auth/SessionManager.hpp"
#include "types/RefreshToken.hpp"
#include "database/Queries/UserQueries.hpp"
#include "logging/LogRegistry.hpp"

using namespace vh::auth;
using namespace vh::database;
using namespace vh::logging;

void SessionManager::createSession(const std::shared_ptr<Client>& client) {
    std::lock_guard lock(sessionMutex_);

    if (!client || !client->getSession()) throw std::invalid_argument("Session must not be null");

    activeSessions_[client->getSession()->getUUID()] = client;
    LogRegistry::ws()->debug("[SessionManager] Created session for user: {}", client->getUserName());
}

std::string SessionManager::promoteSession(const std::shared_ptr<Client>& client) {
    try {
        std::lock_guard lock(sessionMutex_);

        if (!client || !client->getSession()) throw std::invalid_argument("Client and session must not be null");

        const auto refreshToken = client->getRefreshToken();
        refreshToken->setUserId(client->getUser()->id);
        refreshToken->setUserAgent(client->getSession()->getUserAgent());
        refreshToken->setIpAddress(client->getSession()->getClientIp());

        if (const auto dbToken = UserQueries::getRefreshToken(refreshToken->getJti())) {
            if (dbToken->getUserId() != client->getUser()->id
                || dbToken->getUserAgent() != client->getSession()->getUserAgent())
                throw std::invalid_argument("Invalid refresh token");
        } else UserQueries::addRefreshToken(refreshToken);

        client->setRefreshToken(UserQueries::getRefreshToken(refreshToken->getJti()));
        activeSessions_[client->getSession()->getUUID()] = client;

        LogRegistry::ws()->debug("[SessionManager] Promoted session for user: {}", client->getUserName());
        return client->getRawToken();
    } catch (const std::exception& e) {
        LogRegistry::ws()->error("[SessionManager] Failed to promote session: {}", e.what());
        return "";
    }
}

std::shared_ptr<Client> SessionManager::getClientSession(const std::string& UUID) {
    std::lock_guard lock(sessionMutex_);
    if (activeSessions_.contains(UUID)) return activeSessions_[UUID];
    return nullptr;
}

void SessionManager::invalidateSession(const std::string& sessionUUID) {
    std::lock_guard lock(sessionMutex_);

    if (activeSessions_.contains(sessionUUID)) {
        if (const auto user = activeSessions_[sessionUUID]->getUser()) {
            activeSessions_[sessionUUID]->invalidateToken();
            UserQueries::revokeAndPurgeRefreshTokens(user->id);
            LogRegistry::ws()->debug("[SessionManager] Invalidated session: {}", sessionUUID);
        }
        activeSessions_.erase(sessionUUID);
    }
}

std::unordered_map<std::string, std::shared_ptr<Client> > SessionManager::getActiveSessions() {
    std::lock_guard lock(sessionMutex_);
    return activeSessions_;
}
