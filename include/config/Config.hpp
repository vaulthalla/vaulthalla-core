#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <nlohmann/json_fwd.hpp>

namespace vh::config {

struct ServerConfig {
    std::string host = "0.0.0.0";
    uint16_t port = 8080;
    std::filesystem::path uds_socket = "/tmp/vaulthalla.sock";
    std::string log_level = "info";
    int max_connections = 1024;
};

struct FuseConfig {
    bool enabled = true;
    std::filesystem::path root_mount_path = "/mnt/vaulthalla";
    std::filesystem::path backing_path = "/var/lib/vaulthalla";
    bool mount_per_user = true;
    int fuse_timeout_seconds = 60;
    bool allow_other = true;
    unsigned int admin_linux_uid = 0;
};

struct LoggingConfig {
    std::filesystem::path log_dir = "/var/log/vaulthalla";
    std::chrono::days log_rotation_days = std::chrono::days(30);
};

struct PDFDocumentConfig {
    bool enabled = true;
    unsigned int max_pages = 0;
    unsigned int expiry_days = 15;
};

struct DocumentPreviewConfig {
    PDFDocumentConfig pdf;
};

struct PreviewConfig {
    DocumentPreviewConfig documents;
};

struct ThumbnailsConfig {
    std::vector<std::string> formats = {"jpg", "jpeg", "png", "webp", "pdf"};
    std::vector<unsigned int> sizes = {128, 256, 512};
    unsigned int expiry_days = 30;
};

struct FullSizeCacheConfig {
    bool mirror = true;
    unsigned int expiry_days = 7;
};

struct SourceCacheFlags {
    bool thumbnails = true;
    struct {
        bool pdf = true;
    } documents;
};

struct CachingConfig {
    std::filesystem::path path = ".cache";
    unsigned int max_size_mb = 10240;

    SourceCacheFlags cloud;
    SourceCacheFlags local;

    FullSizeCacheConfig cloud_preview;
    ThumbnailsConfig thumbnails;
    PreviewConfig previews;
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
    std::filesystem::path mount_point = "/data/vaulthalla";
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
    bool dev_mode = false;
    bool init_dev_r2_test_vault = false;
};

struct Config {
    inline static const std::filesystem::path CONFIG_FILE_PATH = "/etc/vaulthalla";

    ServerConfig server;
    FuseConfig fuse;
    LoggingConfig logging;
    CachingConfig caching;
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

} // namespace vh::config
