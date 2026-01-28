#include "protocols/websocket/handlers/SettingsHandler.hpp"
#include "types/User.hpp"
#include "protocols/websocket/WebSocketSession.hpp"
#include "config/ConfigRegistry.hpp"
#include "config/Config.hpp"

#include <nlohmann/json.hpp>

using namespace vh::websocket;

json SettingsHandler::get(const WebSocketSession& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageSettings())
        throw std::runtime_error("Permission denied: Only admins can view settings");

    return {{"settings", config::ConfigRegistry::get()}};
}

json SettingsHandler::update(const json& payload, const WebSocketSession& session) {
    if (const auto user = session.getAuthenticatedUser(); !user || !user->canManageSettings())
        throw std::runtime_error("Permission denied: Only admins can update settings");

    const config::Config config(payload);
    config.save();
    return {{"settings", config}};
}
