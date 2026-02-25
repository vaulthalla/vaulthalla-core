#pragma once

#include <nlohmann/json_fwd.hpp>

namespace vh::protocols::ws { class Session; }

namespace vh::protocols::ws::handler {

using json = nlohmann::json;

struct Groups {
    static json add(const json& payload, const Session& session);
    static json remove(const json& payload, const Session& session);
    static json addMember(const json& payload, const Session& session);
    static json removeMember(const json& payload, const Session& session);
    static json update(const json& payload, const Session& session);
    static json get(const json& payload, const Session& session);
    static json getByName(const json& payload, const Session& session);
    static json listByUser(const json& payload, const Session& session);
    static json list(const Session& session);
};

}
