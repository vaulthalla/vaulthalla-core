#pragma once

#include "auth/User.hpp"

#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace vh::auth {

    class SessionManager {
    public:
        std::string createSession(std::shared_ptr<User> user);
        std::shared_ptr<User> getUserForSession(const std::string& token);
        void invalidateSession(const std::string& token);

        // For admin / debug: list active sessions
        std::unordered_map<std::string, std::string> listActiveSessions();

    private:
        std::unordered_map<std::string, std::shared_ptr<User>> activeSessions_;
        std::mutex sessionMutex_;

        std::string generateRandomToken();
    };

} // namespace vh::auth
