#pragma once

#include "auth/SessionManager.hpp"
#include "protocols/websocket/WebSocketRouter.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <memory>

namespace vh::auth {
class AuthManager;
}

namespace vh::websocket {
class NotificationBroadcastManager;

namespace asio = boost::asio;
namespace beast = boost::beast;
using tcp = asio::ip::tcp;

class WebSocketServer : public std::enable_shared_from_this<WebSocketServer> {
public:
    WebSocketServer(asio::io_context& ioc, const tcp::endpoint& endpoint,
                    const std::shared_ptr<WebSocketRouter>& router,
                    const std::shared_ptr<auth::AuthManager>& authManager);

    void run();

private:
    tcp::acceptor acceptor_;
    asio::io_context& ioc_;

    std::shared_ptr<WebSocketRouter> router_;
    std::shared_ptr<auth::AuthManager> authManager_;
    std::shared_ptr<auth::SessionManager> sessionManager_;
    std::shared_ptr<NotificationBroadcastManager> broadcastManager_;

    void doAccept();
    void onAccept(tcp::socket socket);
};

} // namespace vh::websocket
