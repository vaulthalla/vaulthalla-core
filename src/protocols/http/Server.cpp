#include "protocols/http/Server.hpp"
#include "protocols/http/Session.hpp"
#include "protocols/http/task/AsyncSession.hpp"
#include "concurrency/ThreadPoolManager.hpp"

using namespace vh::protocols::http;
using namespace vh::concurrency;

Server::Server(net::io_context& ioc, const tcp::endpoint& endpoint)
    : TcpServerBase(ioc, endpoint, protocols::TcpServerOptions{
          .acceptConcurrency = 1,
          .useStrand = true,
          .channel = protocols::LogChannel::Http
      }) {}

void Server::onAccept(tcp::socket socket) {
    auto session = std::make_shared<Session>(std::move(socket));
    ThreadPoolManager::instance().httpPool()->submit(std::make_unique<task::AsyncSession>(session));
}
