#include "websocket/WebSocketServer.hpp"
#include "websocket/WebSocketSession.hpp"
#include "websocket/handlers/NotificationBroadcastManager.hpp"

#include <iostream>

namespace vh::websocket {

    WebSocketServer::WebSocketServer(asio::io_context& ioc,
                                     const tcp::endpoint& endpoint,
                                     WebSocketRouter& router,
                                     const std::shared_ptr<vh::auth::SessionManager>& sessionManager)
            : acceptor_(ioc), socket_(ioc), ioc_(ioc),
              router_(router), sessionManager_(sessionManager),
              broadcastManager_(std::make_shared<vh::websocket::NotificationBroadcastManager>()) {
        beast::error_code ec;

        acceptor_.open(endpoint.protocol(), ec);
        if (ec) {
            throw std::runtime_error("Failed to open acceptor: " + ec.message());
        }

        acceptor_.set_option(asio::socket_base::reuse_address(true), ec);
        if (ec) {
            throw std::runtime_error("Failed to set reuse_address: " + ec.message());
        }

        acceptor_.bind(endpoint, ec);
        if (ec) {
            throw std::runtime_error("Failed to bind acceptor: " + ec.message());
        }

        acceptor_.listen(asio::socket_base::max_listen_connections, ec);
        if (ec) {
            throw std::runtime_error("Failed to listen on acceptor: " + ec.message());
        }
    }

    void WebSocketServer::run() {
        std::cout << "[WebSocketServer] Starting to accept connections on " << acceptor_.local_endpoint() << "\n";
        doAccept();
    }

    void WebSocketServer::doAccept() {
        acceptor_.async_accept(socket_, [self = shared_from_this()](boost::system::error_code ec) {
            self->onAccept(ec);
        });
    }

    void WebSocketServer::onAccept(boost::system::error_code ec) {
        if (ec) {
            std::cerr << "[WebSocketServer] Accept error: " << ec.message() << "\n";
        } else {
            std::cout << "[WebSocketServer] Accepted new connection.\n";

            // Launch WebSocketSession
            std::make_shared<WebSocketSession>(std::move(socket_), router_, sessionManager_, broadcastManager_)->run();
        }

        // Accept next connection
        doAccept();
    }

} // namespace vh::websocket
