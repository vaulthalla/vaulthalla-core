#include "websocket/WebSocketRouter.hpp"
#include "auth/SessionManager.hpp"
#include "websocket/WebSocketSession.hpp"
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

        std::string command = msg.at("command").get<std::string>();
        std::string accessToken = msg.value("token", "");

        if (!command.starts_with("auth")) {
            auto client = sessionManager_->getClientSession(session.getUUID());
            if (!client || !client->validateToken(accessToken)) {
                std::cerr << "[Router] Unauthorized access attempt with command: " << command << std::endl;
                json errorResponse = {{"command", "error"},
                                      {"status", "unauthorized"},
                                      {"message", "You must be authenticated to perform this action."}};
                session.send(errorResponse);
                return;
            }
        }

        auto it = handlers_.find(command);
        if (it != handlers_.end()) it->second(msg, session);
        else {
            std::cerr << "[Router] Unknown command: " << command << std::endl;
            // send an error back here too
        }
    } catch (const std::exception& e) {
        std::cerr << "[Router] Error routing message: " << e.what() << std::endl;
    }
}

} // namespace vh::websocket
