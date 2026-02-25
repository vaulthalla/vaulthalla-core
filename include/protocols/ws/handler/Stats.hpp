#pragma once

#include "nlohmann/json_fwd.hpp"

namespace vh::protocols::ws { class Session; }

namespace vh::protocols::ws::handler {

using json = nlohmann::json;

struct Stats {
    static json vault(const json& payload, const Session& session);
    static json fsCache(const Session& session);
    static json httpCache(const Session& session);
};

}
