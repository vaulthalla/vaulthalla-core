#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <memory>

namespace vh::auth { class AuthManager; }
namespace vh::storage { class StorageManager; }
namespace vh::services { class ServiceManager; }

namespace vh::http {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;
class HttpRouter;

class HttpServer : public std::enable_shared_from_this<HttpServer> {
public:
    HttpServer(net::io_context& ioc, const tcp::endpoint& endpoint);

    void run();

private:
    void do_accept();

    tcp::acceptor acceptor_;
    tcp::socket socket_;
    std::shared_ptr<HttpRouter> router_;
    std::shared_ptr<auth::AuthManager> authManager_;
    std::shared_ptr<storage::StorageManager> storageManager_;
};

} // namespace vh::http
