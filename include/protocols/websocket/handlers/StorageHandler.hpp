#pragma once

#include <nlohmann/json_fwd.hpp>

namespace vh::websocket {

using json = nlohmann::json;

class WebSocketSession;

struct StorageHandler {
    // API commands
    static json addAPIKey(const json& payload, const WebSocketSession& session);
    static json removeAPIKey(const json& payload, const WebSocketSession& session);
    static json listAPIKeys(const WebSocketSession& session);
    static json getAPIKey(const json& payload, const WebSocketSession& session);

    // All Vault commands
    static json listVaults(const WebSocketSession& session);
    static json addVault(const json& payload, const WebSocketSession& session);
    static json updateVault(const json& payload, const WebSocketSession& session);
    static json removeVault(const json& payload, const WebSocketSession& session);
    static json getVault(const json& payload, const WebSocketSession& session);
    static json syncVault(const json& payload, const WebSocketSession& session);
};

} // namespace vh::websocket
