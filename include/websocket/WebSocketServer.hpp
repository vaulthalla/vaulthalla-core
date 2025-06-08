#pragma once

#include "websocket/WebSocketRouter.hpp"
#include "auth/SessionManager.hpp"

#include <boost/beast/core.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/io_context.hpp>

#include <memory>

namespace vh::websocket {

    namespace asio = boost::asio;
    namespace beast = boost::beast;
    using tcp = asio::ip::tcp;

    class WebSocketServer : public std::enable_shared_from_this<WebSocketServer> {
    public:
        WebSocketServer(asio::io_context& ioc,
                        tcp::endpoint endpoint,
                        WebSocketRouter& router,
                        auth::SessionManager& sessionManager);

        void run();

    private:
        tcp::acceptor acceptor_;
        tcp::socket socket_;
        asio::io_context& ioc_;

        WebSocketRouter& router_;
        auth::SessionManager& sessionManager_;

        void doAccept();
        void onAccept(boost::system::error_code ec);
    };

} // namespace vh::websocket
