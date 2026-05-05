#pragma once

#include "nlohmann/json_fwd.hpp"
#include <memory>

namespace vh::protocols::ws { class Session; }

namespace vh::protocols::ws::handler {

using json = nlohmann::json;

struct Stats {
    static json vault(const json& payload, const std::shared_ptr<Session>& session);
    static json vaultSync(const json& payload, const std::shared_ptr<Session>& session);
    static json vaultActivity(const json& payload, const std::shared_ptr<Session>& session);
    static json vaultShares(const json& payload, const std::shared_ptr<Session>& session);
    static json vaultSecurity(const json& payload, const std::shared_ptr<Session>& session);
    static json systemHealth(const std::shared_ptr<Session>& session);
    static json systemThreadPools(const std::shared_ptr<Session>& session);
    static json systemFuse(const std::shared_ptr<Session>& session);
    static json systemDb(const std::shared_ptr<Session>& session);
    static json fsCache(const std::shared_ptr<Session>& session);
    static json httpCache(const std::shared_ptr<Session>& session);
};

}
