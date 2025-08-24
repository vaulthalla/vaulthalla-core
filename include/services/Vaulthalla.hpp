#pragma once

#include "services/AsyncService.hpp"

#include <memory>
#include <boost/asio/ip/tcp.hpp>

namespace vh::websocket {
class WebSocketServer;
class WebSocketRouter;
class WebSocketHandler;
}

namespace boost::asio {
class io_context;
}

namespace vh::http {
class HttpServer;
}

namespace vh::services {

class ConnectionLifecycleManager;

class Vaulthalla final : public AsyncService {
public:
    Vaulthalla();

protected:
    void runLoop() override;

private:
    std::thread ioThread_;
    std::shared_ptr<boost::asio::io_context> ioContext_;
    std::shared_ptr<websocket::WebSocketRouter> wsRouter_;
    std::shared_ptr<websocket::WebSocketHandler> wsHandler_;
    std::shared_ptr<websocket::WebSocketServer> wsServer_;
    std::shared_ptr<http::HttpServer> httpServer_;

    void initProtocols();
    void initWebsocketServer();
    void initHttpServer();
    void initThreatIntelligence();
};

} // namespace vh::services