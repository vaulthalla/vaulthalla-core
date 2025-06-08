#include "auth/SessionManager.hpp"

#include <sodium.h>
#include <iostream>
#include <sstream>
#include <iomanip>

namespace vh::auth {

    std::string SessionManager::createSession(std::shared_ptr<User> user) {
        std::lock_guard<std::mutex> lock(sessionMutex_);

        std::string token = generateRandomToken();
        activeSessions_[token] = user;

        std::cout << "[SessionManager] Created session for user: "
                  << user->getUsername() << " token: " << token << "\n";

        return token;
    }

    std::shared_ptr<User> SessionManager::getUserForSession(const std::string& token) {
        std::lock_guard<std::mutex> lock(sessionMutex_);

        auto it = activeSessions_.find(token);
        if (it != activeSessions_.end()) {
            return it->second;
        }

        return nullptr;
    }

    void SessionManager::invalidateSession(const std::string& token) {
        std::lock_guard<std::mutex> lock(sessionMutex_);

        auto it = activeSessions_.find(token);
        if (it != activeSessions_.end()) {
            std::cout << "[SessionManager] Invalidated session for user: "
                      << it->second->getUsername() << "\n";
            activeSessions_.erase(it);
        }
    }

    std::unordered_map<std::string, std::string> SessionManager::listActiveSessions() {
        std::lock_guard<std::mutex> lock(sessionMutex_);

        std::unordered_map<std::string, std::string> result;
        for (const auto& pair : activeSessions_) {
            result[pair.first] = pair.second->getUsername();
        }

        return result;
    }

    std::string SessionManager::generateRandomToken() {
        unsigned char buf[16]; // 128-bit token → plenty
        randombytes_buf(buf, sizeof buf);

        std::ostringstream oss;
        for (size_t i = 0; i < sizeof buf; ++i) {
            oss << std::hex << std::setw(2) << std::setfill('0') << (int)buf[i];
        }

        return oss.str();
    }

} // namespace vh::auth
