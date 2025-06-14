#include "auth/SessionManager.hpp"
#include "crypto/PasswordHash.hpp"
#include "database/Queries/UserQueries.hpp"
#include "auth/RefreshToken.hpp"

#include <iostream>

namespace vh::auth {

    void SessionManager::createSession(const std::shared_ptr<Client>& client) {
        std::lock_guard<std::mutex> lock(sessionMutex_);

        if (!client || !client->getSession()) throw std::invalid_argument("Session and user must not be null");

        activeSessions_[client->getHashedRefreshToken()] = client;
        std::cout << "[SessionManager] Created new stateless session.\n";
    }

    std::string SessionManager::promoteSession(const std::shared_ptr<Client>& client) {
        std::lock_guard<std::mutex> lock(sessionMutex_);

        if (!client || !client->getSession()) throw std::invalid_argument("Client and session must not be null");

        auto refreshToken = client->getRefreshToken();
        refreshToken->setUserId(client->getUser()->id);
        refreshToken->setUserAgent(client->getSession()->getUserAgent());
        refreshToken->setIpAddress(client->getSession()->getClientIp());
        vh::database::UserQueries::addRefreshToken(refreshToken);
        client->setRefreshToken(vh::database::UserQueries::getRefreshToken(refreshToken->getJti()));
        activeSessions_[refreshToken->getHashedToken()] = client;

        std::cout << "[SessionManager] Promoted session for user: " << client->getEmail() << "\n";
        return client->getRawToken();
    }

    std::shared_ptr<Client> SessionManager::getClientSession(const std::string& token) {
        std::lock_guard<std::mutex> lock(sessionMutex_);
        auto it = activeSessions_.find(token);
        if (it != activeSessions_.end()) return it->second;

        return nullptr;
    }

    void SessionManager::invalidateSession(const std::string& token) {
        std::lock_guard<std::mutex> lock(sessionMutex_);

        auto it = activeSessions_.find(token);
        if (it != activeSessions_.end()) {
            it->second->invalidateToken();
            std::cout << "[SessionManager] Invalidated session for user: "
                      << it->second->getEmail() << "\n";
            activeSessions_.erase(it);
        }
    }

    std::unordered_map<std::string, std::shared_ptr<Client>> SessionManager::getActiveSessions() {
        std::lock_guard<std::mutex> lock(sessionMutex_);
        return activeSessions_;
    }

} // namespace vh::auth
