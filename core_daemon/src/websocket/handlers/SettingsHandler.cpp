#include "websocket/handlers/SettingsHandler.hpp"
#include "types/db/User.hpp"
#include "websocket/WebSocketSession.hpp"
#include "types/config/ConfigRegistry.hpp"
#include "types/config/Config.hpp"

#include <nlohmann/json.hpp>

using namespace vh::websocket;

void SettingsHandler::handleGetSettings(const json& msg, WebSocketSession& session) {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user || !user->canManageSettings())
            throw std::runtime_error("Permission denied: Only admins can view settings");

        const json data = {{"settings", types::config::ConfigRegistry::get()}};

        const json response = {
            {"command", "settings.get.response"},
            {"status", "ok"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"data", data}
        };

        session.send(response);
    } catch (const std::exception& e) {
        const json response = {
            {"command", "settings.get.response"},
            {"status", "error"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"error", e.what()}
        };
        session.send(response);
    }
}

void SettingsHandler::handleUpdateSettings(const json& msg, WebSocketSession& session) {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user || !user->canManageSettings())
            throw std::runtime_error("Permission denied: Only admins can update settings");

        const auto& payload = msg.at("payload");
        const types::config::Config config(payload);
        config.save();

        const json response = {
            {"command", "settings.update.response"},
            {"status", "ok"},
            {"requestId", msg.at("requestId").get<std::string>()}
        };

        session.send(response);
    } catch (const std::exception& e) {
        const json response = {
            {"command", "settings.update.response"},
            {"status", "error"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"error", e.what()}
        };
        session.send(response);
    }
}
