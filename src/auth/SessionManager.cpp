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

        auto refreshToken = client->getRefreshToken();
        refreshToken->setUserId(client->getUser()->id);
        refreshToken->setUserAgent(client->getSession()->getUserAgent());
        refreshToken->setIpAddress(client->getSession()->getClientIp());
        UserQueries::addRefreshToken(refreshToken);
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
    auto it = activeSessions_.find(UUID);
    if (it != activeSessions_.end()) return it->second;

    return nullptr;
}

void SessionManager::invalidateSession(const std::string& sessionUUID) {
    std::lock_guard lock(sessionMutex_);

    auto it = activeSessions_.find(sessionUUID);
    if (it != activeSessions_.end()) {
        if (it->second->getUser()) {
            it->second->invalidateToken();
            UserQueries::revokeAndPurgeRefreshTokens(it->second->getUser()->id);
            LogRegistry::ws()->debug("[SessionManager] Invalidated session: {}", sessionUUID);
        }
        activeSessions_.erase(it);
    }
}

std::unordered_map<std::string, std::shared_ptr<Client> > SessionManager::getActiveSessions() {
    std::lock_guard lock(sessionMutex_);
    return activeSessions_;
}
