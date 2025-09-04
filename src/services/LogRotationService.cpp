#include "services/LogRotationService.hpp"
#include "logging/LogRegistry.hpp"
#include "logging/LogRotator.hpp"
#include "config/ConfigRegistry.hpp"

#include <paths.h>

using namespace vh::services;
using namespace vh::logging;
using namespace vh::config;

static constexpr auto CHECK_INTERVAL = std::chrono::minutes(10);

const auto& cfg = ConfigRegistry::get().auditing.audit_log;

inline static LogRotator appRot({
    .active_path = vh::paths::getLogPath() / "vaulthalla.log",
    .max_bytes = 25_MiB,
    .max_interval = std::chrono::hours(24),
    .retention_days = std::chrono::days(7),
    .compression = LogRotator::Compression::Zstd,
    .ignore_compress_errors = true,
    .on_reopen = []() { LogRegistry::reopenMainLog(); },
    .diag_log = [](const std::string_view& msg) { LogRegistry::vaulthalla()->debug("[LogRotator] {}", msg); }
});

inline static LogRotator auditRot({
    .active_path = vh::paths::getLogPath() / "audit.log",
    .max_bytes = cfg.rotate_max_size,
    .max_interval = cfg.rotate_interval,
    .retention_days = cfg.retention_days,
    .max_retained_size = cfg.max_retained_logs_size,
    .strict_retention = cfg.strict_retention,
    .compression = cfg.compression,
    .ignore_compress_errors = true,
    .on_reopen = []() { LogRegistry::reopenAuditLog(); },
    .diag_log = [](const std::string_view& msg) { LogRegistry::audit()->debug("[AuditLogRotator] {}", msg); }
});

LogRotationService::LogRotationService() : AsyncService("LogRotationService") {
    std::this_thread::sleep_for(std::chrono::seconds(5));
}

LogRotationService::~LogRotationService() { stop(); }

void LogRotationService::runLoop() {
    while (isRunning() && !interruptFlag_.load()) {
        std::this_thread::sleep_for(CHECK_INTERVAL);
        LogRegistry::vaulthalla()->debug("[LogRotationService] Checking for log rotation...");
        appRot.maybeRotate();
        auditRot.maybeRotate();
    }
}
