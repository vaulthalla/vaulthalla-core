#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <spdlog/spdlog.h>
#include <nlohmann/json_fwd.hpp>

namespace vh::config {

constexpr static uintmax_t MAX_UPLOAD_SIZE_BYTES = static_cast<uintmax_t>(2) * 1024 * 1024 * 1024; // 2GB
constexpr static uintmax_t MAX_PREVIEW_SIZE_BYTES = 100 * 1024 * 1024;        // 100MB

struct WebsocketConfig {
    bool enabled = true;
    std::string host = "0.0.0.0";
    uint16_t port = 33369;
    unsigned int max_connections = 1024;
    uintmax_t max_upload_size_bytes = MAX_UPLOAD_SIZE_BYTES;
};

struct HttpPreviewConfig {
    bool enabled = true;
    std::string host = "0.0.0.0";
    uint16_t port = 33370;
    unsigned int max_connections = 512;
    uintmax_t max_preview_size_bytes = MAX_PREVIEW_SIZE_BYTES;
};

struct SubsystemLogLevelsConfig {
    spdlog::level::level_enum vaulthalla = spdlog::level::debug;
    spdlog::level::level_enum fuse = spdlog::level::debug;
    spdlog::level::level_enum filesystem = spdlog::level::info;
    spdlog::level::level_enum crypto = spdlog::level::info;
    spdlog::level::level_enum cloud = spdlog::level::info;
    spdlog::level::level_enum auth = spdlog::level::info;
    spdlog::level::level_enum websocket = spdlog::level::info;
    spdlog::level::level_enum http = spdlog::level::info;
    spdlog::level::level_enum shell = spdlog::level::info;
    spdlog::level::level_enum db = spdlog::level::warn;
    spdlog::level::level_enum sync = spdlog::level::info;
    spdlog::level::level_enum thumb = spdlog::level::info;
    spdlog::level::level_enum storage = spdlog::level::info;
    spdlog::level::level_enum types = spdlog::level::info;
};

struct LogLevelsConfig {
    spdlog::level::level_enum console_log_level = spdlog::level::info;
    spdlog::level::level_enum file_log_level = spdlog::level::debug;
    SubsystemLogLevelsConfig subsystem_levels;
};

struct LoggingConfig {
    std::chrono::days log_rotation_days = std::chrono::days(7);
    std::chrono::days audit_log_rotation_days = std::chrono::days(30);
    LogLevelsConfig levels;
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
    int pool_size = 10;
};

struct AuthConfig {
    unsigned int token_expiry_minutes = 60;
    unsigned int refresh_token_expiry_days = 7;
};

struct SharingConfig {
    bool enabled = true;
    bool enable_public_links = true;
};

struct DevConfig {
    bool enabled = false;
    bool init_r2_test_vault = false;
};

struct Config {
    WebsocketConfig websocket;
    HttpPreviewConfig http_preview;
    LoggingConfig logging;
    CachingConfig caching;
    DatabaseConfig database;
    AuthConfig auth;
    SharingConfig sharing;
    DevConfig dev;

    void save() const;
};

Config loadConfig(const std::string& path);
void to_json(nlohmann::json& j, const Config& c);
void from_json(const nlohmann::json& j, Config& c);
void to_json(nlohmann::json& j, const WebsocketConfig& c);
void from_json(const nlohmann::json& j, WebsocketConfig& c);
void to_json(nlohmann::json& j, const HttpPreviewConfig& c);
void from_json(const nlohmann::json& j, HttpPreviewConfig& c);
void to_json(nlohmann::json& j, const LogLevelsConfig& c);
void from_json(const nlohmann::json& j, LogLevelsConfig& c);
void to_json(nlohmann::json& j, const SubsystemLogLevelsConfig& c);
void from_json(const nlohmann::json& j, SubsystemLogLevelsConfig& c);
void to_json(nlohmann::json& j, const LoggingConfig& c);
void from_json(const nlohmann::json& j, LoggingConfig& c);
void to_json(nlohmann::json& j, const PDFDocumentConfig& c);
void from_json(const nlohmann::json& j, PDFDocumentConfig& c);
void to_json(nlohmann::json& j, const DocumentPreviewConfig& c);
void from_json(const nlohmann::json& j, DocumentPreviewConfig& c);
void to_json(nlohmann::json& j, const PreviewConfig& c);
void from_json(const nlohmann::json& j, PreviewConfig& c);
void to_json(nlohmann::json& j, const ThumbnailsConfig& c);
void from_json(const nlohmann::json& j, ThumbnailsConfig& c);
void to_json(nlohmann::json& j, const FullSizeCacheConfig& c);
void from_json(const nlohmann::json& j, FullSizeCacheConfig& c);
void to_json(nlohmann::json& j, const SourceCacheFlags& c);
void from_json(const nlohmann::json& j, SourceCacheFlags& c);
void to_json(nlohmann::json& j, const CachingConfig& c);
void from_json(const nlohmann::json& j, CachingConfig& c);
void to_json(nlohmann::json& j, const DatabaseConfig& c);
void from_json(const nlohmann::json& j, DatabaseConfig& c);
void to_json(nlohmann::json& j, const AuthConfig& c);
void from_json(const nlohmann::json& j, AuthConfig& c);
void to_json(nlohmann::json& j, const SharingConfig& c);
void from_json(const nlohmann::json& j, SharingConfig& c);
void to_json(nlohmann::json& j, const DevConfig& c);
void from_json(const nlohmann::json& j, DevConfig& c);

} // namespace vh::config
