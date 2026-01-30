#pragma once

#include "protocols/TcpServerBase.hpp"

namespace vh::http {

namespace net = boost::asio;
using tcp = net::ip::tcp;

class HttpServer final : public protocols::TcpServerBase {
public:
    HttpServer(net::io_context& ioc, const tcp::endpoint& endpoint);

private:
    std::string_view serverName() const noexcept override { return "HttpServer"; }
    void onAccept(tcp::socket socket) override;
};

} // namespace vh::http
