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
#include "logging/LogRegistry.hpp"

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
    if (const auto& cfg = ConfigRegistry::get(); !cfg.websocket.enabled && !cfg.http_preview.enabled) {
        LogRegistry::vaulthalla()->warn(
            "[Vaulthalla] Both WebSocket and HTTP preview servers are disabled in configuration. Nothing to start.");
        return;
    }

    ioContext_ = std::make_shared<boost::asio::io_context>();

    initWebsocketServer();
    initHttpServer();

    ioThread_ = std::thread([ctx = ioContext_] { ctx->run(); });
}


void Vaulthalla::initWebsocketServer() {
    const auto& cfg = ConfigRegistry::get().websocket;
    if (!cfg.enabled) {
        LogRegistry::vaulthalla()->info("[Vaulthalla] WebSocket server is disabled in configuration.");
        return;
    }

    const auto endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address(cfg.host), cfg.port);
    wsRouter_ = std::make_shared<websocket::WebSocketRouter>();
    wsHandler_ = std::make_shared<websocket::WebSocketHandler>(wsRouter_);
    wsServer_ = std::make_shared<websocket::WebSocketServer>(*ioContext_, endpoint, wsRouter_);

    wsServer_->run();
}

void Vaulthalla::initHttpServer() {
    const auto& cfg = ConfigRegistry::get().http_preview;
    if (!cfg.enabled) {
        LogRegistry::vaulthalla()->info("[Vaulthalla] HTTP preview server is disabled in configuration.");
        return;
    }

    const auto endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address(cfg.host), cfg.port);
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
