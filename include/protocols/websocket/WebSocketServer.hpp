#pragma once
#include "protocols/TcpServerBase.hpp"
#include <memory>

namespace vh::websocket {

class WebSocketRouter;

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

class WebSocketServer final : public protocols::TcpServerBase {
public:
    WebSocketServer(asio::io_context& ioc, const tcp::endpoint& endpoint,
                    std::shared_ptr<WebSocketRouter> router);

private:
    std::string_view serverName() const noexcept override { return "WebSocketServer"; }
    void onAccept(tcp::socket socket) override;

    std::shared_ptr<WebSocketRouter> router_;
};

}
