#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <unordered_set>
#include <memory>
#include <string>

namespace vh::auth {
class AuthManager;
}

namespace vh::storage {
class StorageManager;
}

namespace vh::services {
class ServiceManager;
}

namespace vh::http {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

class HttpPreviewServer : public std::enable_shared_from_this<HttpPreviewServer> {
public:
    HttpPreviewServer(net::io_context& ioc, const tcp::endpoint& endpoint,
                      const std::shared_ptr<services::ServiceManager>& serviceManager);

    void run();

private:
    void do_accept();

    void handle_session(tcp::socket socket) const;

    std::string map_request_to_file(unsigned int vault_id, const std::string& rel_path) const;

    tcp::acceptor acceptor_;
    tcp::socket socket_;
    std::shared_ptr<auth::AuthManager> authManager_;
    std::shared_ptr<storage::StorageManager> storageManager_;

    // Mock session store for now
    std::unordered_set<std::string> valid_sessions_ = {"abc123", "def456"};
};

} // namespace vh::http