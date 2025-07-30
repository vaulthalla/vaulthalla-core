#include "protocols/http/HttpServer.hpp"
#include "protocols/http/HttpSession.hpp"
#include "protocols/http/HttpRouter.hpp"
#include "services/ServiceManager.hpp"
#include "protocols/http/HttpSessionTask.hpp"
#include "concurrency/ThreadPool.hpp"
#include "concurrency/ThreadPoolRegistry.hpp"

#include <iostream>

using namespace vh::concurrency;
using namespace vh::services;

namespace vh::http {

HttpServer::HttpServer(net::io_context& ioc, const tcp::endpoint& endpoint,
                                     const std::shared_ptr<ServiceManager>& serviceManager)
    : acceptor_(ioc), socket_(ioc),
      router_(std::make_shared<HttpRouter>(serviceManager->authManager, serviceManager->storageManager)),
      authManager_(serviceManager->authManager),
      storageManager_(serviceManager->storageManager) {
    beast::error_code ec;
    acceptor_.open(endpoint.protocol(), ec);
    if (ec) throw beast::system_error(ec);
    acceptor_.set_option(net::socket_base::reuse_address(true), ec);
    if (ec) throw beast::system_error(ec);
    acceptor_.bind(endpoint, ec);
    if (ec) throw beast::system_error(ec);
    acceptor_.listen(net::socket_base::max_listen_connections, ec);
    if (ec) throw beast::system_error(ec);
}

void HttpServer::run() {
    std::cout << "[HttpServer] Preview server listening on " << acceptor_.local_endpoint() << std::endl;
    do_accept();
}

void HttpServer::do_accept() {
    acceptor_.async_accept(socket_, [self = shared_from_this()](beast::error_code ec) mutable {
        if (!ec) {
            auto session = std::make_shared<HttpSession>(
                std::move(self->socket_), self->router_, self->authManager_, self->storageManager_
            );
            ThreadPoolRegistry::instance().httpPool()->submit(std::make_unique<HttpSessionTask>(std::move(session)));
        }
        self->do_accept();
    });
}

} // namespace vh::http
