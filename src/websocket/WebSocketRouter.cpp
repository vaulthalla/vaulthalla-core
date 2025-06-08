#include "websocket/WebSocketRouter.hpp"
#include <iostream>

namespace vh::websocket {

    void WebSocketRouter::registerHandler(const std::string& command, HandlerFunc handler) {
        handlers_[command] = std::move(handler);
    }

    void WebSocketRouter::routeMessage(const json& msg, WebSocketSession& session) {
        try {
            std::string command = msg.at("command").get<std::string>();

            auto it = handlers_.find(command);
            if (it != handlers_.end()) {
                it->second(msg, session);
            } else {
                std::cerr << "[Router] Unknown command: " << command << "\n";
                // You could send an error back here too
            }
        } catch (const std::exception& e) {
            std::cerr << "[Router] Error routing message: " << e.what() << "\n";
        }
    }

} // namespace vh::websocket
