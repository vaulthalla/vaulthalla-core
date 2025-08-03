#include "logging/Logger.hpp"
#include <spdlog/sinks/basic_file_sink.h>
#include <filesystem>

namespace vh::logging {

void Logger::init(const std::string& logDir, spdlog::level::level_enum logLevel) {
    namespace fs = std::filesystem;
    if (!fs::exists(logDir)) {
        fs::create_directories(logDir);
    }

    // --- Core logger ---
    std::vector<spdlog::sink_ptr> coreSinks;

    // Console sink (colored, stderr for errors)
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_level(logLevel);
    coreSinks.push_back(consoleSink);

    // Rotating file sink for ops logs
    auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        logDir + "/vaulthalla.log", 1024 * 1024 * 10, 5); // 10MB x 5 files
    fileSink->set_level(logLevel);
    coreSinks.push_back(fileSink);

#ifdef __linux__
    // Optional: syslog sink for journald/systemd integration
    try {
        auto syslogSink = std::make_shared<spdlog::sinks::syslog_sink_mt>("vaulthalla", LOG_PID, LOG_USER, true);
        syslogSink->set_level(logLevel);
        coreSinks.push_back(syslogSink);
    } catch (const spdlog::spdlog_ex& e) {
        std::cerr << "[Logger] Failed to init syslog sink: " << e.what() << std::endl;
    }
#endif

    coreLogger_ = std::make_shared<spdlog::logger>("core", begin(coreSinks), end(coreSinks));
    spdlog::register_logger(coreLogger_);
    coreLogger_->set_level(logLevel);
    coreLogger_->flush_on(spdlog::level::warn);

    // --- Audit logger ---
    auto auditSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        logDir + "/audit.log", true /* append */);

    auditSink->set_level(spdlog::level::info); // always at least info
    auditLogger_ = std::make_shared<spdlog::logger>("audit", auditSink);
    spdlog::register_logger(auditLogger_);
    auditLogger_->set_level(spdlog::level::info);
    auditLogger_->flush_on(spdlog::level::info);
}

std::shared_ptr<spdlog::logger> Logger::core() {
    return coreLogger_;
}

std::shared_ptr<spdlog::logger> Logger::audit() {
    return auditLogger_;
}

}
