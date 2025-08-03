#pragma once

#include <memory>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/syslog_sink.h>

namespace vh::logging {

class Logger {
public:
    static void init(const std::string& logDir = "/var/log/vaulthalla",
                     spdlog::level::level_enum logLevel = spdlog::level::info);

    static std::shared_ptr<spdlog::logger> core();
    static std::shared_ptr<spdlog::logger> audit();

private:
    inline static std::shared_ptr<spdlog::logger> coreLogger_;
    inline static std::shared_ptr<spdlog::logger> auditLogger_;
};

}
