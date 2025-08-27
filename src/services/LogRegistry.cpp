#include "services/LogRegistry.hpp"
#include "config/ConfigRegistry.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <filesystem>

namespace vh::logging {

void LogRegistry::init(const std::filesystem::path& logDir) {
    if (initialized_) {
        spdlog::warn("[LogRegistry] Already initialized, ignoring second init()");
        return;
    }

    spdlog::info("[LogRegistry] Initializing... LogDir: {}", logDir.string());

    const auto log_file = logDir / "vaulthalla.log";
    const auto audit_log_file = logDir / "audit.log";

    namespace fs = std::filesystem;
    if (!fs::exists(logDir)) fs::create_directories(logDir);

    const auto cnf = config::ConfigRegistry::get().logging;
    const auto consoleLevel = cnf.levels.console_log_level;
    const auto fileLevel = cnf.levels.file_log_level;

    const auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_level(consoleLevel);
    consoleSink->set_color_mode(spdlog::color_mode::automatic);
    consoleSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");

    const auto rotatingSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(log_file.string(), 1024 * 1024 * 10, 5);
    rotatingSink->set_level(fileLevel);

    auto makeLogger = [&](const std::string& name, const spdlog::level::level_enum lvl = spdlog::level::debug) {
        const auto logger = std::make_shared<spdlog::logger>(name, spdlog::sinks_init_list{consoleSink, rotatingSink});
        logger->set_level(lvl);
        logger->flush_on(spdlog::level::warn);
        spdlog::register_logger(logger);
    };

    const auto& sub_levels = cnf.levels.subsystem_levels;

    makeLogger("vaulthalla", sub_levels.vaulthalla);
    makeLogger("fuse", sub_levels.fuse);
    makeLogger("filesystem", sub_levels.filesystem);
    makeLogger("cloud", sub_levels.cloud);
    makeLogger("crypto", sub_levels.crypto);
    makeLogger("auth", sub_levels.auth);
    makeLogger("ws", sub_levels.websocket);
    makeLogger("http", sub_levels.http);
    makeLogger("shell", sub_levels.shell);
    makeLogger("db", sub_levels.db);
    makeLogger("sync", sub_levels.sync);
    makeLogger("thumb", sub_levels.thumb);
    makeLogger("storage", sub_levels.storage);
    makeLogger("types", sub_levels.types);

    // Audit logger (special: append-only file sink, no rotation)
    {
        const auto auditSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(audit_log_file.string(), true);
        std::vector<spdlog::sink_ptr> sinks = { auditSink };
        const auto logger = std::make_shared<spdlog::logger>("audit", sinks.begin(), sinks.end());
        logger->set_level(spdlog::level::info);
        logger->flush_on(spdlog::level::info);
        spdlog::register_logger(logger);
    }

    initialized_ = true;
    spdlog::info("[LogRegistry] Initialized");
}

std::shared_ptr<spdlog::logger> LogRegistry::get(const std::string& name) {
    auto logger = spdlog::get(name);
    if (!logger) {
        if (!initialized_) throw std::runtime_error("[LogRegistry] LogRegistry not initialized, cannot get logger: " + name);
        throw std::runtime_error("[LogRegistry] Logger not found: " + name);
    }
    return logger;
}

bool LogRegistry::isInitialized() { return initialized_; }

}
