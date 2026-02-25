#pragma once

#include "protocols/TcpServerBase.hpp"

namespace vh::protocols::http {

namespace net = boost::asio;
using tcp = net::ip::tcp;

class Server final : public TcpServerBase {
public:
    Server(net::io_context& ioc, const tcp::endpoint& endpoint);

private:
    std::string_view serverName() const noexcept override { return "HttpServer"; }
    void onAccept(tcp::socket socket) override;
};

} // namespace vh::http
