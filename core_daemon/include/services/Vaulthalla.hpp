#pragma once

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

namespace vh::config {
class Config;
}

namespace vh::services {

class ConnectionLifecycleManager;
class ServiceManager;
class SyncController;

class Vaulthalla {
  public:
    void start();
    void stop();
    void restart();
    [[nodiscard]] bool isRunning() const;

  private:
    std::shared_ptr<config::Config> config_;
    std::shared_ptr<boost::asio::io_context> ioContext_;
    std::shared_ptr<ServiceManager> serviceManager_;

    std::shared_ptr<websocket::WebSocketRouter> wsRouter_;
    std::shared_ptr<websocket::WebSocketHandler> wsHandler_;
    std::shared_ptr<websocket::WebSocketServer> wsServer_;
    std::shared_ptr<http::HttpServer> httpServer_;

    std::shared_ptr<ConnectionLifecycleManager> lifecycleManager_;
    std::shared_ptr<SyncController> syncController_;

    void initConfig();
    void initProtocols();
    void initWebsocketServer(const boost::asio::ip::tcp::endpoint& endpoint);
    void initHttpServer(const boost::asio::ip::tcp::endpoint& endpoint);
    void initServices();
    void initThreatIntelligence();
};

} // namespace vh::services
