#pragma once

#include "auth/Client.hpp"
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace vh::websocket {
class WebSocketSession;
}

namespace vh::auth {

class SessionManager {
  public:
    void createSession(const std::shared_ptr<Client>& client);
    std::string promoteSession(const std::shared_ptr<Client>& client);
    std::shared_ptr<Client> getClientSession(const std::string& token);
    void invalidateSession(const std::string& token);

    // For admin / debug: list active sessions
    std::unordered_map<std::string, std::shared_ptr<Client>> getActiveSessions();

  private:
    std::unordered_map<std::string, std::shared_ptr<Client>> sessionsByUUID_;
    std::unordered_map<std::string, std::shared_ptr<Client>> sessionsByRefreshJti_;
    std::mutex sessionMutex_;
};

inline std::string to_string(const std::unordered_map<std::string, std::shared_ptr<Client>>& sessions) {
    std::string result;
    for (const auto& [token, client] : sessions) result += "Token: " + token + "\n";
    return result;
}

} // namespace vh::auth
