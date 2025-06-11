#include "services/Vaulthalla.hpp"
#include "database/Transactions.hpp"
#include "crypto/PasswordUtils.hpp"
#include <libenvpp/env.hpp>

namespace vh::services {
    void Vaulthalla::start() {
        std::cout << "Vaulthalla service started." << std::endl;

        auto pre = env::prefix("VAULTHALLA");
        const auto ws_addr = pre.register_variable<std::string>("WEBSOCKET_ADDRESS");
        const auto ws_port = pre.register_variable<unsigned short>("WEBSOCKET_PORT");
        const auto parsed = pre.parse_and_validate();

        if (parsed.ok()) {
            std::cout << "[Vaulthalla] WebSocket server starting at "
                      << parsed.get_or(ws_addr, "127.0.0.1") << ":" << parsed.get_or(ws_port, 9001) << "\n";
        } else {
            std::cout << parsed.warning_message();
            std::cout << parsed.error_message();
            return;
        }

        const auto addr = boost::asio::ip::make_address(parsed.get_or(ws_addr, "127.0.0.1"));
        const unsigned short port = parsed.get_or(ws_port, 9001);

        try {
            ioContext_ = std::make_shared<boost::asio::io_context>();

            serviceManager_ = std::make_shared<vh::services::ServiceManager>();

            lifecycleManager_ = std::make_shared<vh::services::ConnectionLifecycleManager>(
                    serviceManager_->authManager()->sessionManager()
            );

            wsRouter_ = std::make_shared<vh::websocket::WebSocketRouter>();

            wsHandler_ = std::make_shared<vh::websocket::WebSocketHandler>(serviceManager_, wsRouter_);

            wsServer_ = std::make_shared<vh::websocket::WebSocketServer>(
                    *ioContext_,
                    boost::asio::ip::tcp::endpoint(addr, port),
                    wsRouter_,
                    serviceManager_->authManager()->sessionManager()
            );

            vh::database::Transactions::init();

            wsServer_->run();

            ioContext_->run();

            vh::auth::PasswordUtils::loadCommonWeakPasswordsFromURLs({
                "https://raw.githubusercontent.com/danielmiessler/SecLists/refs/heads/master/Passwords/Common-Credentials/100k-most-used-passwords-NCSC.txt",
                "https://raw.githubusercontent.com/danielmiessler/SecLists/refs/heads/master/Passwords/Common-Credentials/probable-v2_top-12000.txt"
            });

            vh::auth::PasswordUtils::loadDictionaryFromURL(
                    "https://raw.githubusercontent.com/dolph/dictionary/refs/heads/master/popular.txt"
            );
        } catch (const std::exception& e) {
            std::cerr << "[Vaulthalla] Exception: " << e.what() << "\n";
        }
    }

    void Vaulthalla::stop() {
        std::cout << "Vaulthalla service stopped." << std::endl;

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
