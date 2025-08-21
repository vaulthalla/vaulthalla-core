#include "services/LogRegistry.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <filesystem>

namespace vh::logging {

void LogRegistry::init(const std::string& logDir) {
    if (initialized_) {
        spdlog::warn("[LogRegistry] Already initialized, ignoring second init()");
        return;
    }

    namespace fs = std::filesystem;
    if (!fs::exists(logDir)) fs::create_directories(logDir);

    constexpr auto defaultLevel = spdlog::level::info; // Default log level

    const auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_level(defaultLevel);
    consoleSink->set_color_mode(spdlog::color_mode::automatic);
    consoleSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");

    // Shared rotating file sink (10MB * 5 files)
    const auto rotatingSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        logDir + "/vaulthalla.log", 1024 * 1024 * 10, 5);
    rotatingSink->set_level(defaultLevel);

    // Helper to register a subsystem logger
    auto makeLogger = [&](const std::string& name, spdlog::level::level_enum lvl) {
        std::vector<spdlog::sink_ptr> sinks;
        sinks.push_back(consoleSink);
        sinks.push_back(rotatingSink);

        const auto logger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
        logger->set_level(lvl);
        logger->flush_on(spdlog::level::warn);
        spdlog::register_logger(logger);
    };

    makeLogger("vaulthalla", spdlog::level::info);
    makeLogger("fuse", spdlog::level::debug);  // FUSE layer is chatty
    makeLogger("filesystem", spdlog::level::info);
    makeLogger("cloud", spdlog::level::info);
    makeLogger("crypto", spdlog::level::info);
    makeLogger("auth", spdlog::level::info);
    makeLogger("ws", spdlog::level::info);
    makeLogger("http", spdlog::level::info);
    makeLogger("shell", spdlog::level::info);
    makeLogger("db", spdlog::level::info);
    makeLogger("sync", spdlog::level::info);
    makeLogger("thumb", spdlog::level::info);
    makeLogger("storage", spdlog::level::info);
    makeLogger("types", spdlog::level::info);

    // Audit logger (special: append-only file sink, no rotation)
    {
        const auto auditSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            logDir + "/audit.log", true /* append */);
        std::vector<spdlog::sink_ptr> sinks = { auditSink };
        const auto logger = std::make_shared<spdlog::logger>("audit", sinks.begin(), sinks.end());
        logger->set_level(spdlog::level::info);
        logger->flush_on(spdlog::level::info);
        spdlog::register_logger(logger);
    }

    initialized_ = true;
}

std::shared_ptr<spdlog::logger> LogRegistry::get(const std::string& name) {
    auto logger = spdlog::get(name);
    if (!logger) throw std::runtime_error("[LogRegistry] Logger not found: " + name);
    return logger;
}

bool LogRegistry::isInitialized() { return initialized_; }

}
