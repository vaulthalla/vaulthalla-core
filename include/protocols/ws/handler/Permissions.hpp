#pragma once

#include <nlohmann/json_fwd.hpp>

namespace vh::protocols::ws { class Session; }

namespace vh::protocols::ws::handler {

using json = nlohmann::json;

struct Permissions {
    static json get(const json& payload, const Session& session);
    static json getByName(const json& payload, const Session& session);
    static json list(const Session& session);
};

}
