#include "protocols/websocket/WebSocketServer.hpp"
#include "protocols/websocket/WebSocketSession.hpp"
#include "protocols/websocket/WebSocketRouter.hpp"
#include "protocols/TcpAcceptorUtil.hpp"

namespace vh::websocket {

WebSocketServer::WebSocketServer(asio::io_context& ioc, const tcp::endpoint& endpoint,
                                 std::shared_ptr<WebSocketRouter> router)
    : TcpServerBase(ioc, endpoint, protocols::TcpServerOptions{
          .acceptConcurrency = 4,
          .useStrand = true,
          .channel = protocols::LogChannel::WebSocket
      })
    , router_(std::move(router)) {}

void WebSocketServer::onAccept(tcp::socket socket) {
    protocols::wrap_sys("[WebSocketServer] TCP_NODELAY set failed",
        [&] { socket.set_option(tcp::no_delay(true)); });

    protocols::wrap_sys("[WebSocketServer] KEEPALIVE set failed",
        [&] { socket.set_option(asio::socket_base::keep_alive(true)); });

    std::make_shared<WebSocketSession>(router_)->accept(std::move(socket));
}

}
