#include "services/Vaulthalla.hpp"
#include "services/SyncController.hpp"
#include "crypto/PasswordUtils.hpp"
#include "database/Transactions.hpp"
#include "config/ConfigRegistry.hpp"
#include "config/Config.hpp"
#include "services/ConnectionLifecycleManager.hpp"
#include "protocols/websocket/WebSocketHandler.hpp"
#include "protocols/websocket/WebSocketRouter.hpp"
#include "protocols/websocket/WebSocketServer.hpp"
#include "services/ServiceManager.hpp"
#include "protocols/http/HttpServer.hpp"
#include "concurrency/ThreadPoolRegistry.hpp"

#include <boost/asio/io_context.hpp>
#include <iostream>

namespace vh::services {
void Vaulthalla::start() {
    std::cout << "Starting Vaulthalla service..." << std::endl;

    try {
        initConfig();
        concurrency::ThreadPoolRegistry::instance().init();
        database::Transactions::init();
        serviceManager_ = std::make_shared<ServiceManager>();
        initServices();
        initThreatIntelligence();
        initProtocols();
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

void Vaulthalla::initConfig() {
    config::ConfigRegistry::init(config::loadConfig("/etc/vaulthalla/config.yaml"));
    config_ = std::make_shared<config::Config>(config::ConfigRegistry::get());
}


void Vaulthalla::initProtocols() {
    const auto addr = boost::asio::ip::make_address(config_->server.host);
    auto port = config_->server.port;

    ioContext_ = std::make_shared<boost::asio::io_context>();

    initWebsocketServer(boost::asio::ip::tcp::endpoint(addr, port++));
    initHttpServer(boost::asio::ip::tcp::endpoint(addr, port));

    ioContext_->run();
}


void Vaulthalla::initWebsocketServer(const boost::asio::ip::tcp::endpoint& endpoint) {
    wsRouter_ = std::make_shared<websocket::WebSocketRouter>(serviceManager_->authManager()->sessionManager());
    wsHandler_ = std::make_shared<websocket::WebSocketHandler>(serviceManager_, wsRouter_);
    wsServer_ = std::make_shared<websocket::WebSocketServer>(*ioContext_, endpoint,
                                                                     wsRouter_, serviceManager_->authManager());

    wsServer_->run();
}

void Vaulthalla::initHttpServer(const boost::asio::ip::tcp::endpoint& endpoint) {
    httpServer_ = std::make_shared<http::HttpServer>(*ioContext_, endpoint, serviceManager_);
    httpServer_->run();
}

void Vaulthalla::initServices() {
    // TODO: fix segfaults in ConnectionLifecycleManager
    // lifecycleManager_ = std::make_shared<ConnectionLifecycleManager>(serviceManager_->authManager()->sessionManager());
    // lifecycleManager_->start();
    serviceManager_->storageManager()->initializeControllers();
}

void Vaulthalla::initThreatIntelligence() {
    auth::PasswordUtils::loadCommonWeakPasswordsFromURLs(
            {"https://raw.githubusercontent.com/danielmiessler/SecLists/refs/heads/master/Passwords/Common-Credentials/"
             "100k-most-used-passwords-NCSC.txt",
             "https://raw.githubusercontent.com/danielmiessler/SecLists/refs/heads/master/Passwords/Common-Credentials/"
             "probable-v2_top-12000.txt"});

    auth::PasswordUtils::loadDictionaryFromURL(
        "https://raw.githubusercontent.com/dolph/dictionary/refs/heads/master/popular.txt");
}



} // namespace vh::services
