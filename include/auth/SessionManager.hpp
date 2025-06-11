#pragma once

#include "types/User.hpp"
#include "auth/Client.hpp"

#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace vh::websocket {
    class WebSocketSession;
}

namespace vh::auth {

    class SessionManager {
    public:
        std::string createSession(const std::shared_ptr<vh::websocket::WebSocketSession>& session,
                                const std::shared_ptr<vh::types::User>& user);
        std::shared_ptr<Client> getClientSession(const std::string& token);
        void invalidateSession(const std::string& token);

        // For admin / debug: list active sessions
        std::unordered_map<std::string, std::shared_ptr<Client>> getActiveSessions();

    private:
        std::unordered_map<std::string, std::shared_ptr<Client>> activeSessions_;
        std::mutex sessionMutex_;
    };

} // namespace vh::auth
