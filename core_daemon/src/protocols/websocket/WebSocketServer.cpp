#include "protocols/websocket/WebSocketServer.hpp"
#include "auth/AuthManager.hpp"
#include "protocols/websocket/WebSocketSession.hpp"
#include "protocols/websocket/handlers/NotificationBroadcastManager.hpp"
#include <iostream>
#include <thread>

namespace vh::websocket {

WebSocketServer::WebSocketServer(asio::io_context& ioc, const tcp::endpoint& endpoint,
                                 const std::shared_ptr<WebSocketRouter>& router,
                                 const std::shared_ptr<auth::AuthManager>& authManager)
    : acceptor_(ioc), ioc_(ioc), router_(router), authManager_(authManager),
      sessionManager_(authManager_->sessionManager()),
      broadcastManager_(std::make_shared<NotificationBroadcastManager>()) {
    beast::error_code ec;

    acceptor_.open(endpoint.protocol(), ec);
    if (ec) throw std::runtime_error("Failed to open acceptor: " + ec.message());

    acceptor_.set_option(asio::socket_base::reuse_address(true), ec);
    if (ec) throw std::runtime_error("Failed to set reuse_address: " + ec.message());

    acceptor_.bind(endpoint, ec);
    if (ec) throw std::runtime_error("Failed to bind acceptor: " + ec.message());

    acceptor_.listen(asio::socket_base::max_listen_connections, ec);
    if (ec) throw std::runtime_error("Failed to listen on acceptor: " + ec.message());
}

void WebSocketServer::run() {
    std::cout << "[WebSocketServer] Starting to accept connections on " << acceptor_.local_endpoint() << std::endl;
    doAccept();
}

void WebSocketServer::doAccept() {
    auto sock = std::make_shared<tcp::socket>(acceptor_.get_executor());
    acceptor_.async_accept(*sock, [self = shared_from_this(), sock](boost::system::error_code ec) {
        if (!ec) self->onAccept(std::move(*sock));
        self->doAccept();
    });
}

void WebSocketServer::onAccept(tcp::socket socket) {
    socket.set_option(tcp::no_delay(true));
    socket.set_option(asio::socket_base::keep_alive(true));

    auto session = std::make_shared<WebSocketSession>(router_, broadcastManager_, authManager_);
    session->accept(std::move(socket));
}

} // namespace vh::websocket
