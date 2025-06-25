#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <nlohmann/json_fwd.hpp>

namespace vh::types::config {

struct ServerConfig {
    std::string host = "0.0.0.0";
    uint16_t port = 8080;
    std::string uds_socket = "/tmp/vaulthalla.sock";
    std::string log_level = "info";
    int max_connections = 1024;
};

struct FuseConfig {
    bool enabled = true;
    std::string root_mount_path = "/mnt/vaulthalla";
    bool mount_per_user = true;
    int fuse_timeout_seconds = 60;
    bool allow_other = true;
};

struct DatabaseConfig {
    std::string host = "localhost";
    uint16_t port = 5432;
    std::string name = "vaulthalla";
    std::string user = "vaulthalla";
    std::string password = "changeme";
    int pool_size = 10;
};

struct LocalStorageConfig {
    std::string mount_point = "/data/vaulthalla";
};

struct S3StorageConfig {
    std::string endpoint = "https://s3.example.com";
    std::string region = "us-east-1";
    std::string access_key = "REPLACE_ME";
    std::string secret_key = "REPLACE_ME";
    std::string bucket = "vaulthalla-bucket";
};

struct AuthConfig {
    int token_expiry_minutes = 60;
    int refresh_token_expiry_days = 7;
    std::string jwt_secret = "changeme-very-secret";
    bool allow_signup = false;
};

struct MetricsConfig {
    bool enabled = true;
    uint16_t port = 9100;
};

struct AdminUIConfig {
    bool enabled = true;
    uint16_t bind_port = 9090;
    std::vector<std::string> allowed_ips = {"127.0.0.1", "::1"};
};

struct SchedulerConfig {
    int cleanup_interval_hours = 24;
    int audit_prune_days = 90;
    int usage_refresh_minutes = 10;
};

struct AdvancedConfig {
    bool enable_file_versioning = true;
    int max_upload_size_mb = 2048;
    bool enable_sharing = true;
    bool enable_public_links = true;
    int rate_limit_per_ip_per_minute = 60;
};

struct Config {
    inline static const std::filesystem::path CONFIG_FILE_PATH = "/etc/vaulthalla";

    ServerConfig server;
    FuseConfig fuse;
    DatabaseConfig database;
    AuthConfig auth;
    MetricsConfig metrics;
    AdminUIConfig admin_ui;
    SchedulerConfig scheduler;
    AdvancedConfig advanced;

    void save() const;
};

Config loadConfig(const std::string& path);
void to_json(nlohmann::json& j, const Config& c);
void from_json(const nlohmann::json& j, Config& c);

} // namespace vh::types::config
