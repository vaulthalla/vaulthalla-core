#include "protocols/websocket/WebSocketRouter.hpp"
#include "auth/SessionManager.hpp"
#include "protocols/websocket/WebSocketSession.hpp"
#include <iostream>

namespace vh::websocket {

WebSocketRouter::WebSocketRouter(const std::shared_ptr<auth::SessionManager>& sessionManager)
    : sessionManager_(sessionManager) {
    if (!sessionManager_) throw std::invalid_argument("SessionManager cannot be null");
}

void WebSocketRouter::registerHandler(const std::string& command, HandlerFunc handler) {
    handlers_[command] = std::move(handler);
}

void WebSocketRouter::routeMessage(const json& msg, WebSocketSession& session) {
    try {
        std::cout << "[Router] Routing message: " << msg.dump() << std::endl;

        const std::string command = msg.at("command").get<std::string>();
        const std::string accessToken = msg.value("token", "");

        if (!command.starts_with("auth")) {
            const auto client = sessionManager_->getClientSession(session.getUUID());
            if (!client || !client->validateToken(accessToken)) {
                std::cerr << "[Router] Unauthorized access attempt with command: " << command << std::endl;
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
            std::cerr << "[Router] Unknown command: " << command << std::endl;
            const json errorResponse = {{"command", "error"},
                                        {"status", "unknown_command"},
                                        {"message", "Unknown command: " + command}};
            session.send(errorResponse);
        }
    } catch (const std::exception& e) {
        std::cerr << "[Router] Error routing message: " << e.what() << std::endl;
    }
}

} // namespace vh::websocket
