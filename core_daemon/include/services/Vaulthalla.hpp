#pragma once

#include "services/ConnectionLifecycleManager.hpp"
#include "websocket/WebSocketHandler.hpp"
#include "websocket/WebSocketRouter.hpp"
#include "websocket/WebSocketServer.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <iostream>

namespace vh::services {
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
};
} // namespace vh::services
