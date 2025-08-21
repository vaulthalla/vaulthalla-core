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
#include "concurrency/ThreadPoolManager.hpp"
#include "services/LogRegistry.hpp"

#include <boost/asio/io_context.hpp>
#include <sodium.h>

using namespace vh::services;
using namespace vh::config;
using namespace vh::logging;

Vaulthalla::Vaulthalla() : AsyncService("Vaulthalla") {}

void Vaulthalla::runLoop() {
    LogRegistry::vaulthalla()->info("[Vaulthalla] Starting Vaulthalla - The Final Cloud...");

    try {
        if (sodium_init() < 0) throw std::runtime_error("libsodium initialization failed");
        initThreatIntelligence();
        initProtocols();
        LogRegistry::vaulthalla()->debug("[Vaulthalla] Protocols initialized successfully");

        while (!interruptFlag_.load()) std::this_thread::sleep_for(std::chrono::seconds(1));

        ioContext_->stop();
        if (ioThread_.joinable()) ioThread_.join();

        exit(EXIT_SUCCESS);
    } catch (const std::exception& e) {
        LogRegistry::vaulthalla()->error("[Vaulthalla] Exception in run loop: {}", e.what());
        exit(EXIT_FAILURE);
    } catch (...) {
        LogRegistry::vaulthalla()->error("[Vaulthalla] Unknown exception in run loop");
        exit(EXIT_FAILURE);
    }
}

void Vaulthalla::initProtocols() {
    const auto& serverConfig = ConfigRegistry::get().server;
    const auto addr = boost::asio::ip::make_address(serverConfig.host);
    auto port = serverConfig.port;

    ioContext_ = std::make_shared<boost::asio::io_context>();

    initWebsocketServer(boost::asio::ip::tcp::endpoint(addr, port++));
    initHttpServer(boost::asio::ip::tcp::endpoint(addr, port));

    ioThread_ = std::thread([ctx = ioContext_] { ctx->run(); });
}


void Vaulthalla::initWebsocketServer(const boost::asio::ip::tcp::endpoint& endpoint) {
    wsRouter_ = std::make_shared<websocket::WebSocketRouter>();
    wsHandler_ = std::make_shared<websocket::WebSocketHandler>(wsRouter_);
    wsServer_ = std::make_shared<websocket::WebSocketServer>(*ioContext_, endpoint, wsRouter_);

    wsServer_->run();
}

void Vaulthalla::initHttpServer(const boost::asio::ip::tcp::endpoint& endpoint) {
    httpServer_ = std::make_shared<http::HttpServer>(*ioContext_, endpoint);
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
