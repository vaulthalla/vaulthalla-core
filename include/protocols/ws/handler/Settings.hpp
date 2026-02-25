#pragma once

#include <nlohmann/json_fwd.hpp>

namespace vh::protocols::ws { class Session; }

namespace vh::protocols::ws::handler {

using json = nlohmann::json;

struct Settings {
    static json get(const Session& session);
    static json update(const json& payload, const Session& session);
};

}
