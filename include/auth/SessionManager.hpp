#pragma once

#include "types/User.hpp"

#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace vh::auth {

    class SessionManager {
    public:
        std::string createSession(const std::shared_ptr<vh::types::User>& user);
        std::shared_ptr<vh::types::User> getUserForSession(const std::string& token);
        void invalidateSession(const std::string& token);

        // For admin / debug: list active sessions
        std::unordered_map<std::string, std::string> listActiveSessions();

    private:
        std::unordered_map<std::string, std::shared_ptr<vh::types::User>> activeSessions_;
        std::mutex sessionMutex_;

        static std::string generateRandomToken();
    };

} // namespace vh::auth
