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
    bool isRunning() const;

  private:
    std::shared_ptr<boost::asio::io_context> ioContext_;

    std::shared_ptr<vh::services::ServiceManager> serviceManager_;
    std::shared_ptr<vh::websocket::WebSocketRouter> wsRouter_;
    std::shared_ptr<vh::websocket::WebSocketHandler> wsHandler_;
    std::shared_ptr<vh::websocket::WebSocketServer> wsServer_;
    std::shared_ptr<vh::services::ConnectionLifecycleManager> lifecycleManager_;

    const boost::asio::ip::address address_ =
        std::getenv("VAULTHALLA_WEBSOCKET_ADDRESS")
            ? boost::asio::ip::make_address(std::getenv("VAULTHALLA_WEBSOCKET_ADDRESS"))
            : boost::asio::ip::make_address("127.0.0.1");

    const unsigned short port_ = std::getenv("VAULTHALLA_WEBSOCKET_PORT")
                                     ? static_cast<unsigned short>(std::stoi(std::getenv("VAULTHALLA_WEBSOCKET_PORT")))
                                     : 9001;
};
} // namespace vh::services
