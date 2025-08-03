#include "logging/LogRegistry.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/syslog_sink.h>

#include <filesystem>
#include <iostream>

namespace vh::logging {

void LogRegistry::init(const std::string& logDir) {
    if (initialized_) {
        spdlog::warn("[LogRegistry] Already initialized, ignoring second init()");
        return;
    }

    namespace fs = std::filesystem;
    if (!fs::exists(logDir)) {
        fs::create_directories(logDir);
    }

    const auto defaultLevel = spdlog::level::info; // Default log level

    // Shared console sink
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_level(defaultLevel);

    // Shared rotating file sink (10MB * 5 files)
    auto rotatingSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        logDir + "/vaulthalla.log", 1024 * 1024 * 10, 5);
    rotatingSink->set_level(defaultLevel);

#ifdef __linux__
    // Syslog sink (optional, don’t kill if missing)
    std::shared_ptr<spdlog::sinks::syslog_sink_mt> syslogSink;
    try {
        syslogSink = std::make_shared<spdlog::sinks::syslog_sink_mt>(
            "vaulthalla", LOG_PID, LOG_USER, true);
        syslogSink->set_level(defaultLevel);
    } catch (const spdlog::spdlog_ex& e) {
        std::cerr << "[LogRegistry] Failed to init syslog sink: " << e.what() << std::endl;
    }
#endif

    // Helper to register a subsystem logger
    auto makeLogger = [&](const std::string& name, spdlog::level::level_enum lvl) {
        std::vector<spdlog::sink_ptr> sinks;
        sinks.push_back(consoleSink);
        sinks.push_back(rotatingSink);
#ifdef __linux__
        if (syslogSink) sinks.push_back(syslogSink);
#endif
        auto logger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
        logger->set_level(lvl);
        logger->flush_on(spdlog::level::warn);
        spdlog::register_logger(logger);
    };

    makeLogger("fuse", spdlog::level::debug);  // FUSE layer is chatty
    makeLogger("filesystem", spdlog::level::debug); // Filesystem operations
    makeLogger("auth", spdlog::level::info); // Authentication events
    makeLogger("ws", spdlog::level::debug); // WebSocket events
    makeLogger("http", spdlog::level::debug); // HTTP server events
    makeLogger("db", spdlog::level::debug); // Database operations
    makeLogger("vaulthalla", spdlog::level::info); // Core application events
    makeLogger("sync", spdlog::level::info); // Sync operations
    makeLogger("thumb", spdlog::level::debug); // Thumbnail generation
    makeLogger("storage", spdlog::level::debug); // Storage operations

    // Audit logger (special: append-only file sink, no rotation)
    {
        auto auditSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            logDir + "/audit.log", true /* append */);
        std::vector<spdlog::sink_ptr> sinks = { auditSink };
        auto logger = std::make_shared<spdlog::logger>("audit", sinks.begin(), sinks.end());
        logger->set_level(spdlog::level::info);
        logger->flush_on(spdlog::level::info);
        spdlog::register_logger(logger);
    }

    initialized_ = true;
}

std::shared_ptr<spdlog::logger> LogRegistry::get(const std::string& name) {
    auto logger = spdlog::get(name);
    if (!logger) {
        throw std::runtime_error("[LogRegistry] Logger not found: " + name);
    }
    return logger;
}

}
