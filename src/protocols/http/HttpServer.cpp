#include "protocols/http/HttpServer.hpp"
#include "protocols/http/HttpSession.hpp"
#include "protocols/http/HttpSessionTask.hpp"
#include "concurrency/ThreadPoolManager.hpp"

using namespace vh::concurrency;

namespace vh::http {

HttpServer::HttpServer(net::io_context& ioc, const tcp::endpoint& endpoint)
    : TcpServerBase(ioc, endpoint, protocols::TcpServerOptions{
          .acceptConcurrency = 1,
          .useStrand = true,
          .channel = protocols::LogChannel::Http
      }) {}

void HttpServer::onAccept(tcp::socket socket) {
    auto session = std::make_shared<HttpSession>(std::move(socket));
    ThreadPoolManager::instance().httpPool()->submit(std::make_unique<HttpSessionTask>(session));
}

}
