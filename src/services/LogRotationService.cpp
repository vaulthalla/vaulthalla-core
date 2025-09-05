#include "services/LogRotationService.hpp"
#include "logging/LogRegistry.hpp"
#include "logging/LogRotator.hpp"
#include "config/ConfigRegistry.hpp"

#include <paths.h>

using namespace vh::services;
using namespace vh::logging;
using namespace vh::config;

static constexpr auto CHECK_INTERVAL = std::chrono::minutes(10);

LogRotationService::LogRotationService()
    : AsyncService("LogRotationService"),
      appRot_(std::make_unique<LogRotator>(LogRotator::Options{
        .active_path = paths::getLogPath() / "vaulthalla.log",
        .max_bytes = 25_MiB,
        .max_interval = std::chrono::hours(24),
        .retention_days = std::chrono::days(7),
        .max_retained_size = 100_MiB,
        .compression = LogRotator::Compression::Zstd,
        .ignore_compress_errors = true,
        .on_reopen = []() { LogRegistry::reopenMainLog(); },
        .diag_log = [](const std::string_view& msg) { LogRegistry::vaulthalla()->debug("[LogRotator] {}", msg); }
      })),
      auditRot_(std::make_unique<LogRotator>(LogRotator::Options{
        .active_path = paths::getLogPath() / "audit.log",
        .max_bytes = ConfigRegistry::get().auditing.audit_log.rotate_max_size,
        .max_interval = ConfigRegistry::get().auditing.audit_log.rotate_interval,
        .retention_days = ConfigRegistry::get().auditing.audit_log.retention_days,
        .max_retained_size = ConfigRegistry::get().auditing.audit_log.max_retained_logs_size,
        .strict_retention = ConfigRegistry::get().auditing.audit_log.strict_retention,
        .compression = ConfigRegistry::get().auditing.audit_log.compression,
        .ignore_compress_errors = true,
        .on_reopen = []() { LogRegistry::reopenAuditLog(); },
        .diag_log = [](const std::string_view& msg) { LogRegistry::audit()->debug("[AuditLogRotator] {}", msg); }
      })) {}

LogRotationService::~LogRotationService() { stop(); }

void LogRotationService::runLoop() {
    while (isRunning() && !interruptFlag_.load()) {
        std::this_thread::sleep_for(CHECK_INTERVAL);
        LogRegistry::vaulthalla()->debug("[LogRotationService] Checking for log rotation...");
        appRot_->maybeRotate();
        auditRot_->maybeRotate();
    }
}
