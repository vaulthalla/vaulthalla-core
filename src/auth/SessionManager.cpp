#include "auth/SessionManager.hpp"
#include "websocket/WebSocketSession.hpp"
#include "crypto/PasswordHash.hpp"

#include <iostream>

namespace vh::auth {

    std::string SessionManager::createSession(const std::shared_ptr<Client>& client) {
        std::lock_guard<std::mutex> lock(sessionMutex_);

        if (!client || !client->getSession()) throw std::invalid_argument("Session and user must not be null");

        activeSessions_[client->getHashedRefreshToken()] = client;

        client->getSession()->setAuthenticatedUser(client->getUser());
        std::cout << "[SessionManager] Authenticated session for user: " << client->getEmail() << "\n";

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
