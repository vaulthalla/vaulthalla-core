#include "protocols/websocket/WebSocketServer.hpp"

#include "auth/AuthManager.hpp"
#include "protocols/websocket/WebSocketSession.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "logging/LogRegistry.hpp"

#include <boost/asio/strand.hpp>

using namespace vh::services;
using namespace vh::logging;

namespace vh::websocket {

static void wrap(const std::string_view what, auto&& fn) {
    try { fn(); }
    catch (const boost::system::system_error& e) {
        throw std::runtime_error(std::string(what) + ": " + e.what());
    }
}

WebSocketServer::WebSocketServer(asio::io_context& ioc, const tcp::endpoint& endpoint,
                                 const std::shared_ptr<WebSocketRouter>& router)
    : acceptor_(ioc), ioc_(ioc), router_(router) {

    wrap("Failed to open acceptor", [&]{ acceptor_.open(endpoint.protocol()); });
    wrap("Failed to set reuse_address", [&]{ acceptor_.set_option(asio::socket_base::reuse_address(true)); });
    wrap("Failed to bind acceptor", [&]{ acceptor_.bind(endpoint); });
    wrap("Failed to listen on acceptor", [&]{ acceptor_.listen(asio::socket_base::max_listen_connections); });
}

void WebSocketServer::run() {
    LogRegistry::ws()->info("[WebSocketServer] Starting WebSocket server at {}",
        acceptor_.local_endpoint().address().to_string() + ":" +
        std::to_string(acceptor_.local_endpoint().port()));

    constexpr int kAcceptConcurrency = 4; // tune: usually = io threads, or 2–8
    for (unsigned int i = 0; i < kAcceptConcurrency; ++i) doAccept();
}

void WebSocketServer::doAccept() {
    auto self = shared_from_this();

    acceptor_.async_accept(
        asio::make_strand(ioc_), // ensures accept handlers serialize if ioc is multithreaded
        [self](const boost::system::error_code& ec, tcp::socket socket) {
            // Re-arm accept ASAP (minimizes any gap)
            self->doAccept();

            if (ec) {
                if (ec == asio::error::operation_aborted) return; // server shutting down
                LogRegistry::ws()->debug("[WebSocketServer] accept error: {}", ec.message());
                return;
            }

            self->onAccept(std::move(socket));
        });
}

void WebSocketServer::onAccept(tcp::socket socket) {
    boost::system::error_code ec;

    wrap(std::format("[WebSocketServer] TCP_NODELAY set failed: {}", ec.message()),
        [&] { socket.set_option(tcp::no_delay(true), ec); });

    wrap(std::format("[WebSocketServer] KEEPALIVE set failed: {}", ec.message()),
        [&] { socket.set_option(asio::socket_base::keep_alive(true), ec); });

    std::make_shared<WebSocketSession>(router_)->accept(std::move(socket));
}

} // namespace vh::websocket
