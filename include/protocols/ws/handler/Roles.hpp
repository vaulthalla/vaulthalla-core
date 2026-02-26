#pragma once

#include <nlohmann/json_fwd.hpp>

namespace vh::protocols::ws { class Session; }

namespace vh::protocols::ws::handler {

using json = nlohmann::json;

struct Roles {
    static json add(const json& payload, const Session& session);
    static json remove(const json& payload, const Session& session);
    static json update(const json& payload, const Session& session);
    static json get(const json& payload, const Session& session);
    static json getByName(const json& payload, const Session& session);

    static json list(const Session& session);
    static json listUserRoles(const Session& session);
    static json listVaultRoles(const Session& session);
};

}
