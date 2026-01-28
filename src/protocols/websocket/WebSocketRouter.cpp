#include "protocols/websocket/WebSocketRouter.hpp"

#include "auth/AuthManager.hpp"
#include "auth/SessionManager.hpp"
#include "logging/LogRegistry.hpp"
#include "protocols/websocket/WebSocketSession.hpp"
#include "protocols/websocket/WSHandler.hpp"
#include "services/ServiceDepsRegistry.hpp"

using namespace vh::services;
using namespace vh::logging;

namespace vh::websocket {

WebSocketRouter::WebSocketRouter()
    : sessionManager_(ServiceDepsRegistry::instance().authManager->sessionManager()) {
    if (!sessionManager_) throw std::invalid_argument("SessionManager cannot be null");
}

void WebSocketRouter::registerWs(std::string cmd, RawWsHandler fn) {
    // wrapper owns cmd + msg lifecycle
    handlers_[cmd] = makeWsHandler(cmd, std::move(fn));
}

void WebSocketRouter::registerPayload(std::string cmd, RawPayloadHandler fn) {
    handlers_[cmd] = makePayloadHandler(cmd, std::move(fn));
}

void WebSocketRouter::registerHandlerWithToken(std::string cmd, RawHandlerWithToken fn) {
    handlers_[cmd] = makeHandlerWithToken(cmd, std::move(fn));
}

void WebSocketRouter::registerSessionOnlyHandler(std::string cmd, RawSessionOnly fn) {
    handlers_[cmd] = makeSessionOnlyHandler(cmd, std::move(fn));
}

void WebSocketRouter::registerEmptyHandler(std::string cmd, RawEmpty fn) {
    handlers_[cmd] = makeEmptyHandler(cmd, std::move(fn));
}

void WebSocketRouter::registerHandler(std::string cmd, Handler h) {
    handlers_[std::move(cmd)] = std::move(h);
}

void WebSocketRouter::routeMessage(json&& msg, WebSocketSession& session) {
    try {
        LogRegistry::ws()->debug("[Router] Routing message: {}", msg.dump());

        auto command = msg.at("command").get<std::string>();
        const std::string accessToken = msg.value("token", "");

        // Gate non-auth commands
        if (!command.starts_with("auth")) {
            if (const auto client = sessionManager_->getClientSession(session.getUUID());
                !client || !client->validateToken(accessToken)) {

                LogRegistry::ws()->warn("[Router] Unauthorized access attempt for command: {}", command);
                WSResponse::UNAUTHORIZED(std::move(command), std::move(msg))(session);
                return;
            }
        }

        if (handlers_.contains(command)) handlers_[command](std::move(msg), session);
        else {
            LogRegistry::ws()->warn("[Router] Unknown command: {}", command);
            WSResponse::ERROR(std::move(command), std::move(msg), "Unknown command")(session);
        }
    } catch (const std::exception& e) {
        LogRegistry::ws()->error("[Router] Error routing message: {}", e.what());
        WSResponse::INTERNAL_ERROR(std::move(msg), e.what())(session);
    }
}

}
