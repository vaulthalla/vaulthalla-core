#include "services/Vaulthalla.hpp"
#include "crypto/PasswordUtils.hpp"
#include "database/Transactions.hpp"
#include "types/config/ConfigRegistry.hpp"
#include "types/config/Config.hpp"

namespace vh::services {
void Vaulthalla::start() {
    std::cout << "Starting Vaulthalla service..." << std::endl;

    try {
        const auto config = types::config::loadConfig("/etc/vaulthalla/config.yaml");
        types::config::ConfigRegistry::init(config);
        database::Transactions::init();

        ioContext_ = std::make_shared<boost::asio::io_context>();

        serviceManager_ = std::make_shared<ServiceManager>();

        lifecycleManager_ = std::make_shared<ConnectionLifecycleManager>(
            serviceManager_->authManager()->sessionManager());

        wsRouter_ = std::make_shared<websocket::WebSocketRouter>(serviceManager_->authManager()->sessionManager());

        wsHandler_ = std::make_shared<websocket::WebSocketHandler>(serviceManager_, wsRouter_);

        const auto ws_config = config.server;
        const auto addr = boost::asio::ip::make_address(ws_config.host);
        const auto port = ws_config.port;

        wsServer_ = std::make_shared<websocket::WebSocketServer>(*ioContext_,
                                                                     boost::asio::ip::tcp::endpoint(addr, port),
                                                                     wsRouter_, serviceManager_->authManager());

        std::cout << "Websocket listening on: " << addr << ":" << port << std::endl;

        wsServer_->run();

        ioContext_->run();

        auth::PasswordUtils::loadCommonWeakPasswordsFromURLs(
            {"https://raw.githubusercontent.com/danielmiessler/SecLists/refs/heads/master/Passwords/Common-Credentials/"
             "100k-most-used-passwords-NCSC.txt",
             "https://raw.githubusercontent.com/danielmiessler/SecLists/refs/heads/master/Passwords/Common-Credentials/"
             "probable-v2_top-12000.txt"});

        auth::PasswordUtils::loadDictionaryFromURL(
            "https://raw.githubusercontent.com/dolph/dictionary/refs/heads/master/popular.txt");

        std::cout << "Vaulthalla service started." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[Vaulthalla] Exception: " << e.what() << std::endl;
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
} // namespace vh::services
