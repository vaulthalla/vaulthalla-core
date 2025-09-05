#include "protocols/websocket/WebSocketRouter.hpp"
#include "auth/SessionManager.hpp"
#include "protocols/websocket/WebSocketSession.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "logging/LogRegistry.hpp"
#include "auth/AuthManager.hpp"

using namespace vh::services;
using namespace vh::logging;

namespace vh::websocket {

WebSocketRouter::WebSocketRouter()
    : sessionManager_(ServiceDepsRegistry::instance().authManager->sessionManager()) {
    if (!sessionManager_) throw std::invalid_argument("SessionManager cannot be null");
}

void WebSocketRouter::registerHandler(const std::string& command, HandlerFunc handler) {
    handlers_[command] = std::move(handler);
}

void WebSocketRouter::routeMessage(const json& msg, WebSocketSession& session) {
    try {
        LogRegistry::ws()->debug("[Router] Routing message: {}", msg.dump());

        const std::string command = msg.at("command").get<std::string>();
        const std::string accessToken = msg.value("token", "");

        if (!command.starts_with("auth")) {
            const auto client = sessionManager_->getClientSession(session.getUUID());
            if (!client || !client->validateToken(accessToken)) {
                LogRegistry::ws()->warn("[Router] Unauthorized access attempt for command: {}", command);
                const json errorResponse = {{"command", "error"},
                                      {"status", "unauthorized"},
                                      {"message", "You must be authenticated to perform this action."}};
                session.send(errorResponse);
                return;
            }
        }

        const auto it = handlers_.find(command);
        if (it != handlers_.end()) it->second(msg, session);
        else {
            LogRegistry::ws()->warn("[Router] Unknown command: {}", command);
            const json errorResponse = {{"command", "error"},
                                        {"status", "unknown_command"},
                                        {"message", "Unknown command: " + command}};
            session.send(errorResponse);
        }
    } catch (const std::exception& e) {
        LogRegistry::ws()->error("[Router] Error routing message: {}", e.what());
        const json errorResponse = {{"command", "error"},
                                    {"status", "internal_error"},
                                    {"message", "An internal error occurred while processing your request."}};
        session.send(errorResponse);
    }
}

} // namespace vh::websocket
