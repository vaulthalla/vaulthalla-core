#include "protocols/ProtocolService.hpp"
#include "crypto/password/Strength.hpp"
#include "database/Transactions.hpp"
#include "config/ConfigRegistry.hpp"
#include "config/Config.hpp"
#include "protocols/ws/Server.hpp"
#include "protocols/http/Server.hpp"
#include "log/Registry.hpp"

#include <boost/asio/io_context.hpp>
#include <sodium.h>

using namespace vh::protocols;
using namespace vh::config;
using namespace vh::crypto;

ProtocolService::ProtocolService() : AsyncService("Vaulthalla") {}

void ProtocolService::runLoop() {
    log::Registry::vaulthalla()->info("[Vaulthalla] Starting Vaulthalla - The Final Cloud...");

    try {
        if (sodium_init() < 0) throw std::runtime_error("libsodium initialization failed");
        initThreatIntelligence();
        initProtocols();
        log::Registry::vaulthalla()->debug("[Vaulthalla] Protocols initialized successfully");

        while (!shouldStop()) std::this_thread::sleep_for(std::chrono::seconds(1));

        ioContext_->stop();
        if (ioThread_.joinable()) ioThread_.join();

        exit(EXIT_SUCCESS);
    } catch (const std::exception& e) {
        log::Registry::vaulthalla()->error("[Vaulthalla] Exception in run loop: {}", e.what());
        exit(EXIT_FAILURE);
    } catch (...) {
        log::Registry::vaulthalla()->error("[Vaulthalla] Unknown exception in run loop");
        exit(EXIT_FAILURE);
    }
}

void ProtocolService::initProtocols() {
    if (const auto& cfg = ConfigRegistry::get(); !cfg.websocket.enabled && !cfg.http_preview.enabled) {
        log::Registry::vaulthalla()->warn(
            "[Vaulthalla] Both WebSocket and HTTP preview servers are disabled in configuration. Nothing to start.");
        return;
    }

    ioContext_ = std::make_shared<asio::io_context>();

    initWebsocketServer();
    initHttpServer();

    ioThread_ = std::thread([ctx = ioContext_] { ctx->run(); });
}


void ProtocolService::initWebsocketServer() {
    const auto& cfg = ConfigRegistry::get().websocket;
    if (!cfg.enabled) {
        log::Registry::vaulthalla()->info("[Vaulthalla] WebSocket server is disabled in configuration.");
        return;
    }

    const auto endpoint = asio::ip::tcp::endpoint(asio::ip::make_address(cfg.host), cfg.port);
    wsServer_ = std::make_shared<ws::Server>(*ioContext_, endpoint);
    wsServer_->run();
}

void ProtocolService::initHttpServer() {
    const auto& cfg = ConfigRegistry::get().http_preview;
    if (!cfg.enabled) {
        log::Registry::vaulthalla()->info("[Vaulthalla] HTTP preview server is disabled in configuration.");
        return;
    }

    const auto endpoint = asio::ip::tcp::endpoint(asio::ip::make_address(cfg.host), cfg.port);
    httpServer_ = std::make_shared<http::Server>(*ioContext_, endpoint);
    httpServer_->run();
}

void ProtocolService::initThreatIntelligence() {
    password::Strength::loadCommonWeakPasswordsFromURLs(
            {"https://raw.githubusercontent.com/danielmiessler/SecLists/refs/heads/master/Passwords/Common-Credentials/"
             "100k-most-used-passwords-NCSC.txt",
             "https://raw.githubusercontent.com/danielmiessler/SecLists/refs/heads/master/Passwords/Common-Credentials/"
             "probable-v2_top-12000.txt"});

    password::Strength::loadDictionaryFromURL(
        "https://raw.githubusercontent.com/dolph/dictionary/refs/heads/master/popular.txt");
}
