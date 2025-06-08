#include "auth/SessionManager.hpp"
#include "auth/AuthManager.hpp"
#include "auth/TokenValidator.hpp"

#include "core/FSManager.hpp"
#include "index/SearchIndex.hpp"

#include "websocket/WebSocketRouter.hpp"
#include "websocket/WebSocketHandler.hpp"
#include "websocket/WebSocketServer.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <iostream>

int main() {
    try {
        // IO context
        boost::asio::io_context ioc;

        // Core services
        vh::auth::SessionManager sessionManager;
        vh::auth::TokenValidator tokenValidator;
        vh::auth::AuthManager authManager(sessionManager, tokenValidator);

        auto fsManager = std::make_shared<vh::core::FSManager>(); // assuming you have this default constructible
        auto searchIndex = std::make_shared<vh::index::SearchIndex>();

        // WebSocket router
        vh::websocket::WebSocketRouter router;

        // WebSocket handler → registers all routes
        vh::websocket::WebSocketHandler wsHandler(router, sessionManager, authManager, tokenValidator, fsManager, searchIndex);
        wsHandler.registerAllHandlers();

        // WebSocket server
        vh::websocket::WebSocketServer server(ioc,
                                              boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 9001), // Port 9001 for testing
                                              router,
                                              sessionManager);

        server.run();

        std::cout << "[main] WebSocketServer running on port 9001.\n";

        // Run IO loop
        ioc.run();

    } catch (const std::exception& e) {
        std::cerr << "[main] Exception: " << e.what() << "\n";
    }

    return 0;
}
