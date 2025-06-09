#include "services/Vaulthalla.hpp"

namespace vh::services {
    void Vaulthalla::start() {
        std::cout << "Vaulthalla service started." << std::endl;
        try {
            ioContext_ = std::make_shared<boost::asio::io_context>();

            serviceManager_ = std::make_shared<vh::services::ServiceManager>();

            wsRouter_ = std::make_shared<vh::websocket::WebSocketRouter>();

            wsHandler_ = std::make_shared<vh::websocket::WebSocketHandler>(serviceManager_);
            wsHandler_->registerAllHandlers();

            wsServer_ = std::make_shared<vh::websocket::WebSocketServer>(
                    *ioContext_,
                    boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 9001),
                    *wsRouter_,
                    serviceManager_->authManager()->sessionManager()
            );

            wsServer_->run();

            ioContext_->run();
        } catch (const std::exception& e) {
            std::cerr << "[Vaulthalla] Exception: " << e.what() << "\n";
        }
    }

    void Vaulthalla::stop() {
        std::cout << "Vaulthalla service stopped." << std::endl;
        // Clean up resources, stop servers, etc.
    }

    void Vaulthalla::restart() {
        std::cout << "Vaulthalla service restarted." << std::endl;
        stop();
        start();
    }

    bool Vaulthalla::isRunning() const {
        // Placeholder for actual running state check
        return true;
    }
}
