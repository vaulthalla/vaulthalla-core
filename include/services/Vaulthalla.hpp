#pragma once

#include "concurrency/AsyncService.hpp"

#include <memory>
#include <boost/asio/ip/tcp.hpp>

namespace boost::asio { class io_context; }

namespace vh::protocols {
namespace ws { class Server; }
namespace http { class Server; }
}

namespace vh::services {

class ConnectionLifecycleManager;

class Vaulthalla final : public concurrency::AsyncService {
public:
    Vaulthalla();

protected:
    void runLoop() override;

private:
    std::thread ioThread_;
    std::shared_ptr<boost::asio::io_context> ioContext_;
    std::shared_ptr<protocols::ws::Server> wsServer_;
    std::shared_ptr<protocols::http::Server> httpServer_;

    void initProtocols();
    void initWebsocketServer();
    void initHttpServer();
    static void initThreatIntelligence();
};

} // namespace vh::services