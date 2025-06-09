#include "websocket/handlers/NotificationHandler.hpp"
#include "include/auth/User.hpp"
#include <iostream>

namespace vh::websocket {

    void NotificationHandler::handleSubscribe(const json& msg, WebSocketSession& session) {
        try {
            auto user = session.getAuthenticatedUser();
            if (!user) {
                throw std::runtime_error("Unauthorized");
            }

            auto channels = msg.at("channels");
            for (const auto& channel : channels) {
                session.subscribeChannel(channel.get<std::string>());
            }

            json response = {
                    {"command", "notification.subscribe.response"},
                    {"status", "ok"},
                    {"channels", channels}
            };

            session.send(response);

            std::cout << "[NotificationHandler] User '" << user->getUsername()
                      << "' subscribed to channels.\n";

        } catch (const std::exception& e) {
            std::cerr << "[NotificationHandler] handleSubscribe error: " << e.what() << "\n";

            json response = {
                    {"command", "notification.subscribe.response"},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void NotificationHandler::handleUnsubscribe(const json& msg, WebSocketSession& session) {
        try {
            auto user = session.getAuthenticatedUser();
            if (!user) {
                throw std::runtime_error("Unauthorized");
            }

            auto channels = msg.at("channels");
            for (const auto& channel : channels) {
                session.unsubscribeChannel(channel.get<std::string>());
            }

            json response = {
                    {"command", "notification.unsubscribe.response"},
                    {"status", "ok"},
                    {"channels", channels}
            };

            session.send(response);

            std::cout << "[NotificationHandler] User '" << user->getUsername()
                      << "' unsubscribed from channels.\n";

        } catch (const std::exception& e) {
            std::cerr << "[NotificationHandler] handleUnsubscribe error: " << e.what() << "\n";

            json response = {
                    {"command", "notification.unsubscribe.response"},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void NotificationHandler::pushNotification(WebSocketSession& session,
                                               const std::string& channel,
                                               const json& payload) {
        if (!session.isSubscribedTo(channel)) {
            return; // Session not subscribed → no push
        }

        json message = {
                {"command", "notification.push"},
                {"channel", channel},
                {"payload", payload}
        };

        session.send(message);

        std::cout << "[NotificationHandler] Pushed notification on channel '" << channel << "'\n";
    }

} // namespace vh::websocket
