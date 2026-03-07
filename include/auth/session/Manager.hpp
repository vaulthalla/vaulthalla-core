#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace vh::protocols::ws { struct Session; }
namespace vh::identities::model { struct User; }

namespace vh::auth::session {

class Manager {
public:
    using SessionPtr = std::shared_ptr<protocols::ws::Session>;
    using SessionMap = std::unordered_map<std::string, SessionPtr>;
    using SessionUserMap = std::unordered_multimap<uint32_t, SessionPtr>;

    void tryRehydrateSession(const SessionPtr& session);
    void ensureSession(const SessionPtr& session);
    void promoteSession(const SessionPtr& session);

    void cacheSession(const SessionPtr& session);
    void invalidateSession(const std::string& token);
    void invalidateSession(const SessionPtr& session);

    SessionPtr getSession(const std::string& token);
    std::vector<SessionPtr> getSessions(const std::shared_ptr<identities::model::User>& user);
    std::vector<SessionPtr> getSessionsByUserId(uint32_t userId);

    SessionMap getActiveSessions();

private:
    SessionMap sessionsByUUID_;
    SessionMap sessionsByRefreshJti_;
    SessionUserMap sessionsByUserId_;
    std::mutex sessionMutex_;
};

inline std::string to_string(
    const std::unordered_map<std::string, std::shared_ptr<protocols::ws::Session>>& sessions
) {
    std::string result;
    for (const auto& [token, session] : sessions)
        result += "Token: " + token + "\n";
    return result;
}

}
