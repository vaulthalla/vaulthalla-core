#pragma once

#include <memory>

namespace vh::websocket {
    class WebSocketServer;
    class WebSocketRouter;
    class WebSocketHandler;
}

namespace boost::asio {
    class io_context;
}

namespace vh::http {
    class HttpPreviewServer;
}

namespace vh::services {

class ConnectionLifecycleManager;
class ServiceManager;

class Vaulthalla {
  public:
    void start();
    void stop();
    void restart();
    [[nodiscard]] bool isRunning() const;

  private:
    std::shared_ptr<boost::asio::io_context> ioContext_;

    std::shared_ptr<ServiceManager> serviceManager_;
    std::shared_ptr<websocket::WebSocketRouter> wsRouter_;
    std::shared_ptr<websocket::WebSocketHandler> wsHandler_;
    std::shared_ptr<websocket::WebSocketServer> wsServer_;
    std::shared_ptr<ConnectionLifecycleManager> lifecycleManager_;
    std::shared_ptr<http::HttpPreviewServer> httpServer_;
};

} // namespace vh::services
