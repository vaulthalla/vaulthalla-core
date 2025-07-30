#include "services/Vaulthalla.hpp"
#include "crypto/PasswordUtils.hpp"
#include "database/Transactions.hpp"
#include "config/ConfigRegistry.hpp"
#include "config/Config.hpp"
#include "protocols/websocket/WebSocketHandler.hpp"
#include "protocols/websocket/WebSocketRouter.hpp"
#include "protocols/websocket/WebSocketServer.hpp"
#include "services/ServiceManager.hpp"
#include "protocols/http/HttpServer.hpp"
#include "concurrency/ThreadPoolRegistry.hpp"

#include <boost/asio/io_context.hpp>
#include <iostream>
#include <sodium.h>

using namespace vh::services;
using namespace vh::config;

Vaulthalla::Vaulthalla(const std::shared_ptr<ServiceManager>& serviceManager)
    : AsyncService(serviceManager, "Vaulthalla") {}

void Vaulthalla::runLoop() {
    std::cout << "Starting Vaulthalla service..." << std::endl;

    try {
        if (sodium_init() < 0) throw std::runtime_error("libsodium initialization failed");
        initThreatIntelligence();
        initProtocols();
        std::cout << "Vaulthalla service started." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[Vaulthalla] Exception: " << e.what() << std::endl;
    }
}

void Vaulthalla::initProtocols() {
    const auto& serverConfig = ConfigRegistry::get().server;
    const auto addr = boost::asio::ip::make_address(serverConfig.host);
    auto port = serverConfig.port;

    ioContext_ = std::make_shared<boost::asio::io_context>();

    initWebsocketServer(boost::asio::ip::tcp::endpoint(addr, port++));
    initHttpServer(boost::asio::ip::tcp::endpoint(addr, port));

    ioContext_->run();
}


void Vaulthalla::initWebsocketServer(const boost::asio::ip::tcp::endpoint& endpoint) {
    wsRouter_ = std::make_shared<websocket::WebSocketRouter>(serviceManager_->authManager->sessionManager());
    wsHandler_ = std::make_shared<websocket::WebSocketHandler>(serviceManager_, wsRouter_);
    wsServer_ = std::make_shared<websocket::WebSocketServer>(*ioContext_, endpoint,
                                                                     wsRouter_, serviceManager_->authManager);

    wsServer_->run();
}

void Vaulthalla::initHttpServer(const boost::asio::ip::tcp::endpoint& endpoint) {
    httpServer_ = std::make_shared<http::HttpServer>(*ioContext_, endpoint, serviceManager_);
    httpServer_->run();
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
