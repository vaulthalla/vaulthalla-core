#include "protocols/ws/Router.hpp"

#include "auth/AuthManager.hpp"
#include "auth/SessionManager.hpp"
#include "logging/LogRegistry.hpp"
#include "protocols/ws/Session.hpp"
#include "protocols/ws/core/handler_templates.hpp"
#include "services/ServiceDepsRegistry.hpp"

using namespace vh::protocols::ws;
using namespace vh::protocols::ws::core;
using namespace vh::protocols::ws::model;
using namespace vh::services;
using namespace vh::logging;

void Router::registerWs(const std::string& cmd, RawWsHandler fn) {
    // wrapper owns cmd + msg lifecycle
    handlers_[cmd] = makeWsHandler(cmd, std::move(fn));
}

void Router::registerPayload(const std::string& cmd, RawPayloadHandler fn) {
    handlers_[cmd] = makePayloadHandler(cmd, std::move(fn));
}

void Router::registerHandlerWithToken(const std::string& cmd, RawHandlerWithToken fn) {
    handlers_[cmd] = makeHandlerWithToken(cmd, std::move(fn));
}

void Router::registerSessionOnlyHandler(const std::string& cmd, RawSessionOnly fn) {
    handlers_[cmd] = makeSessionOnlyHandler(cmd, std::move(fn));
}

void Router::registerEmptyHandler(const std::string& cmd, RawEmpty fn) {
    handlers_[cmd] = makeEmptyHandler(cmd, std::move(fn));
}

void Router::registerHandler(const std::string& cmd, Handler h) {
    handlers_[cmd] = std::move(h);
}

void Router::routeMessage(json&& msg, Session& session) {
    try {
        LogRegistry::ws()->debug("[Router] Routing message: {}", msg.dump());

        auto command = msg.at("command").get<std::string>();
        const std::string accessToken = msg.value("token", "");

        // Gate non-auth commands
        if (!command.starts_with("auth")) {
            if (const auto client = ServiceDepsRegistry::instance().authManager->sessionManager()->getClientSession(session.getUUID());
                !client || !client->validateToken(accessToken)) {

                LogRegistry::ws()->warn("[Router] Unauthorized access attempt for command: {}", command);
                Response::UNAUTHORIZED(std::move(command), std::move(msg))(session);
                return;
            }
        }

        if (handlers_.contains(command)) handlers_[command](std::move(msg), session);
        else {
            LogRegistry::ws()->warn("[Router] Unknown command: {}", command);
            Response::ERROR(std::move(command), std::move(msg), "Unknown command")(session);
        }
    } catch (const std::exception& e) {
        LogRegistry::ws()->error("[Router] Error routing message: {}", e.what());
        Response::INTERNAL_ERROR(std::move(msg), e.what())(session);
    }
}
