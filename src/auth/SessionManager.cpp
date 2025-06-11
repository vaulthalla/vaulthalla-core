#include "auth/SessionManager.hpp"
#include "websocket/WebSocketSession.hpp"

#include <iostream>

namespace vh::auth {

    std::string SessionManager::createSession(const std::shared_ptr<vh::websocket::WebSocketSession>& session,
                                            const std::shared_ptr<vh::types::User>& user) {
        std::lock_guard<std::mutex> lock(sessionMutex_);

        if (!session || !user) throw std::invalid_argument("Session and user must not be null");

        auto client = std::make_shared<Client>(user, session);
        activeSessions_[client->getRawToken()] = client;

        session->setAuthenticatedUser(user);
        std::cout << "[SessionManager] Authenticated session for user: " << user->email << "\n";

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
