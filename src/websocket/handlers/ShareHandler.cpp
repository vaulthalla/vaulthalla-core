#include "websocket/handlers/ShareHandler.hpp"
#include "websocket/WebSocketSession.hpp"
#include "share/LinkResolver.hpp"
#include "share/ShareLink.hpp"
#include "auth/User.hpp"
#include <iostream>

namespace vh::websocket {

    ShareHandler::ShareHandler(std::shared_ptr<vh::share::LinkResolver> linkResolver)
            : linkResolver_(std::move(linkResolver)) {}

    void ShareHandler::handleCreateLink(const json& msg, WebSocketSession& session) {
        try {
            auto user = session.getAuthenticatedUser();
            if (!user) {
                throw std::runtime_error("Unauthorized");
            }

            std::string mountName = msg.at("mountName").get<std::string>();
            std::string path = msg.at("path").get<std::string>();
            const auto& permissions = msg.at("permissions"); // assume it's a list of permission strings
            int expiresIn = msg.value("expiresIn", 0); // optional expiration in seconds
            const auto& expiresAt = std::chrono::system_clock::now() + std::chrono::seconds(expiresIn);

            share::ShareLink shareLink(user->getUsername(), mountName, path, permissions.get<std::string>(), expiresAt);
            std::string shareLinkUrl = linkResolver_->createLink(shareLink);

            json response = {
                    {"command", "share.createLink.response"},
                    {"status", "ok"},
                    {"shareLinkUrl", shareLinkUrl}
            };

            session.send(response);

            std::cout << "[ShareHandler] User '" << user->getUsername() << "' created share link for " << path << "\n";

        } catch (const std::exception& e) {
            std::cerr << "[ShareHandler] handleCreateLink error: " << e.what() << "\n";

            json response = {
                    {"command", "share.createLink.response"},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

    void ShareHandler::handleResolveLink(const json& msg, WebSocketSession& session) {
        try {
            std::string linkUrl = msg.at("link").get<std::string>();

            auto shareLink = linkResolver_->resolveLink(linkUrl);
            if (!shareLink.has_value()) {
                throw std::runtime_error("Invalid or expired share link");
            }

            json response = {
                    {"command", "share.resolveLink.response"},
                    {"status", "ok"},
                    {"mountName", shareLink->getMountName()},
                    {"path", shareLink->getPath()},
                    {"permissions", shareLink->getPermissionType()}
            };

            session.send(response);

            std::cout << "[ShareHandler] Resolved share link for path " << shareLink->getPath() << "\n";

        } catch (const std::exception& e) {
            std::cerr << "[ShareHandler] handleResolveLink error: " << e.what() << "\n";

            json response = {
                    {"command", "share.resolveLink.response"},
                    {"status", "error"},
                    {"error", e.what()}
            };

            session.send(response);
        }
    }

} // namespace vh::websocket
