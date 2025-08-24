#pragma once

#include "config/Config.hpp"
#include <yaml-cpp/yaml.h>

namespace YAML {

using namespace vh::config;

template<>
struct convert<WebsocketConfig> {
    static Node encode(const WebsocketConfig& rhs) {
        Node node;
        node["enabled"] = rhs.enabled;
        node["host"] = rhs.host;
        node["port"] = rhs.port;
        node["max_connections"] = rhs.max_connections;
        node["max_upload_size_bytes"] = rhs.max_upload_size_bytes;
        return node;
    }

    static bool decode(const Node& node, WebsocketConfig& rhs) {
        if (!node.IsMap()) return false;
        rhs.enabled = node["enabled"].as<bool>(true);
        rhs.host = node["host"].as<std::string>("0.0.0.0");
        rhs.port = node["port"].as<uint16_t>(8080);
        rhs.max_connections = node["max_connections"].as<unsigned int>(1024);
        rhs.max_upload_size_bytes = node["max_upload_size_mb"].as<uintmax_t>(2048) * 1024 * 1024; // Default 2GB
        return true;
    }
};

template<>
struct convert<HttpPreviewConfig> {
    static Node encode(const HttpPreviewConfig& rhs) {
        Node node;
        node["enabled"] = rhs.enabled;
        node["host"] = rhs.host;
        node["port"] = rhs.port;
        node["max_connections"] = rhs.max_connections;
        node["max_preview_size_bytes"] = rhs.max_preview_size_bytes;
        return node;
    }

    static bool decode(const Node& node, HttpPreviewConfig& rhs) {
        if (!node.IsMap()) return false;
        rhs.enabled = node["enabled"].as<bool>(true);
        rhs.host = node["host"].as<std::string>("0.0.0.0");
        rhs.port = node["port"].as<uint16_t>(8081);
        rhs.max_connections = node["max_connections"].as<unsigned int>(512);
        rhs.max_preview_size_bytes = node["max_preview_size_mb"].as<uintmax_t>(100) * 1024 * 1024; // Default 100MB
        return true;
    }
};

static std::string to_std_string(const spdlog::string_view_t sv) { return {sv.data(), sv.size()}; }

template<>
struct convert<SubsystemLogLevelsConfig> {
    static Node encode(const SubsystemLogLevelsConfig& rhs) {
        Node node;
        node["vaulthalla"]  = to_std_string(spdlog::level::to_string_view(rhs.vaulthalla));
        node["fuse"]        = to_std_string(spdlog::level::to_string_view(rhs.fuse));
        node["filesystem"]  = to_std_string(spdlog::level::to_string_view(rhs.filesystem));
        node["crypto"]      = to_std_string(spdlog::level::to_string_view(rhs.crypto));
        node["cloud"]       = to_std_string(spdlog::level::to_string_view(rhs.cloud));
        node["auth"]        = to_std_string(spdlog::level::to_string_view(rhs.auth));
        node["websocket"]   = to_std_string(spdlog::level::to_string_view(rhs.websocket));
        node["http"]        = to_std_string(spdlog::level::to_string_view(rhs.http));
        node["shell"]       = to_std_string(spdlog::level::to_string_view(rhs.shell));
        node["db"]          = to_std_string(spdlog::level::to_string_view(rhs.db));
        node["sync"]        = to_std_string(spdlog::level::to_string_view(rhs.sync));
        node["thumb"]       = to_std_string(spdlog::level::to_string_view(rhs.thumb));
        node["storage"]     = to_std_string(spdlog::level::to_string_view(rhs.storage));
        node["types"]       = to_std_string(spdlog::level::to_string_view(rhs.types));
        return node;
    }

    static bool decode(const Node& node, SubsystemLogLevelsConfig& rhs) {
        if (!node.IsMap()) return false;
        rhs.vaulthalla = spdlog::level::from_str(node["vaulthalla"].as<std::string>("debug"));
        rhs.fuse = spdlog::level::from_str(node["fuse"].as<std::string>("debug"));
        rhs.filesystem = spdlog::level::from_str(node["filesystem"].as<std::string>("info"));
        rhs.crypto = spdlog::level::from_str(node["crypto"].as<std::string>("info"));
        rhs.cloud = spdlog::level::from_str(node["cloud"].as<std::string>("info"));
        rhs.auth = spdlog::level::from_str(node["auth"].as<std::string>("info"));
        rhs.websocket = spdlog::level::from_str(node["websocket"].as<std::string>("info"));
        rhs.http = spdlog::level::from_str(node["http"].as<std::string>("info"));
        rhs.shell = spdlog::level::from_str(node["shell"].as<std::string>("info"));
        rhs.db = spdlog::level::from_str(node["db"].as<std::string>("warn"));
        rhs.sync = spdlog::level::from_str(node["sync"].as<std::string>("info"));
        rhs.thumb = spdlog::level::from_str(node["thumb"].as<std::string>("info"));
        rhs.storage = spdlog::level::from_str(node["storage"].as<std::string>("info"));
        rhs.types = spdlog::level::from_str(node["types"].as<std::string>("info"));
        return true;
    }
};

template<>
struct convert<LogLevelsConfig> {
    static Node encode(const LogLevelsConfig& rhs) {
        Node node;
        node["console_log_level"] = to_std_string(spdlog::level::to_string_view(rhs.console_log_level));
        node["file_log_level"]    = to_std_string(spdlog::level::to_string_view(rhs.file_log_level));
        node["subsystem_levels"]  = rhs.subsystem_levels;
        return node;
    }

    static bool decode(const Node& node, LogLevelsConfig& rhs) {
        if (!node.IsMap()) return false;
        rhs.console_log_level = spdlog::level::from_str(node["console_log_level"].as<std::string>("info"));
        rhs.file_log_level = spdlog::level::from_str(node["file_log_level"].as<std::string>("debug"));
        rhs.subsystem_levels = node["subsystem_levels"].as<SubsystemLogLevelsConfig>();
        return true;
    }
};

// LoggingConfig
template<>
struct convert<LoggingConfig> {
    static Node encode(const LoggingConfig& rhs) {
        Node node;
        node["log_rotation_days"] = rhs.log_rotation_days.count();
        node["audit_log_rotation_days"] = rhs.audit_log_rotation_days.count();
        node["log_levels"] = rhs.levels;
        return node;
    }

    static bool decode(const Node& node, LoggingConfig& rhs) {
        if (!node.IsMap()) return false;
        rhs.log_rotation_days = std::chrono::days(node["log_rotation_days"].as<unsigned int>(7));
        rhs.audit_log_rotation_days = std::chrono::days(node["audit_log_rotation_days"].as<unsigned int>(30));
        rhs.levels = node["log_levels"].as<LogLevelsConfig>();
        return true;
    }
};

// === PDFDocumentConfig ===
template<>
struct convert<PDFDocumentConfig> {
    static Node encode(const PDFDocumentConfig& rhs) {
        Node node;
        node["enabled"] = rhs.enabled;
        node["max_pages"] = rhs.max_pages;
        node["expiry_days"] = rhs.expiry_days;
        return node;
    }

    static bool decode(const Node& node, PDFDocumentConfig& rhs) {
        if (!node.IsMap()) return false;
        rhs.enabled = node["enabled"].as<bool>(true);
        rhs.max_pages = node["max_pages"].as<unsigned int>(0);
        rhs.expiry_days = node["expiry_days"].as<unsigned int>(15);
        return true;
    }
};

// === DocumentPreviewConfig ===
template<>
struct convert<DocumentPreviewConfig> {
    static Node encode(const DocumentPreviewConfig& rhs) {
        Node node;
        node["pdf"] = rhs.pdf;
        return node;
    }

    static bool decode(const Node& node, DocumentPreviewConfig& rhs) {
        if (!node.IsMap()) return false;
        rhs.pdf = node["pdf"].as<PDFDocumentConfig>();
        return true;
    }
};

// === PreviewConfig ===
template<>
struct convert<PreviewConfig> {
    static Node encode(const PreviewConfig& rhs) {
        Node node;
        node["documents"] = rhs.documents;
        return node;
    }

    static bool decode(const Node& node, PreviewConfig& rhs) {
        if (!node.IsMap()) return false;
        rhs.documents = node["documents"].as<DocumentPreviewConfig>();
        return true;
    }
};

// === ThumbnailsConfig ===
template<>
struct convert<ThumbnailsConfig> {
    static Node encode(const ThumbnailsConfig& rhs) {
        Node node;
        node["formats"] = rhs.formats;
        node["sizes"] = rhs.sizes;
        node["expiry_days"] = rhs.expiry_days;
        return node;
    }

    static bool decode(const Node& node, ThumbnailsConfig& rhs) {
        if (!node.IsMap()) return false;
        rhs.formats = node["formats"].as<std::vector<std::string>>();
        rhs.sizes = node["sizes"].as<std::vector<unsigned int>>();
        rhs.expiry_days = node["expiry_days"].as<unsigned int>(30);
        return true;
    }
};

// === FullSizeCacheConfig ===
template<>
struct convert<FullSizeCacheConfig> {
    static Node encode(const FullSizeCacheConfig& rhs) {
        Node node;
        node["mirror"] = rhs.mirror;
        node["expiry_days"] = rhs.expiry_days;
        return node;
    }

    static bool decode(const Node& node, FullSizeCacheConfig& rhs) {
        if (!node.IsMap()) return false;
        rhs.mirror = node["mirror"].as<bool>(true);
        rhs.expiry_days = node["expiry_days"].as<unsigned int>(7);
        return true;
    }
};

// === SourceCacheFlags (cloud/local) ===
template<>
struct convert<SourceCacheFlags> {
    static Node encode(const SourceCacheFlags& rhs) {
        Node node;
        node["thumbnails"] = rhs.thumbnails;
        Node docs;
        docs["pdf"] = rhs.documents.pdf;
        node["documents"] = docs;
        return node;
    }

    static bool decode(const Node& node, SourceCacheFlags& rhs) {
        if (!node.IsMap()) return false;
        rhs.thumbnails = node["thumbnails"].as<bool>(true);
        if (const auto docs = node["documents"]) rhs.documents.pdf = docs["pdf"].as<bool>(true);
        return true;
    }
};

// === CachingConfig ===
template<>
struct convert<CachingConfig> {
    static Node encode(const CachingConfig& rhs) {
        Node node;
        node["path"] = rhs.path.string();
        node["max_size_mb"] = rhs.max_size_mb;
        node["cloud"] = rhs.cloud;
        node["local"] = rhs.local;
        node["cloud_preview"] = rhs.cloud_preview;
        node["thumbnails"] = rhs.thumbnails;
        node["previews"] = rhs.previews;
        return node;
    }

    static bool decode(const Node& node, CachingConfig& rhs) {
        if (!node.IsMap()) return false;
        rhs.path = node["path"].as<std::string>(".cache");
        rhs.max_size_mb = node["max_size_mb"].as<unsigned int>(10240);
        rhs.cloud = node["cloud"].as<SourceCacheFlags>();
        rhs.local = node["local"].as<SourceCacheFlags>();
        rhs.cloud_preview = node["cloud_preview"].as<FullSizeCacheConfig>();
        rhs.thumbnails = node["thumbnails"].as<ThumbnailsConfig>();
        rhs.previews = node["previews"].as<PreviewConfig>();
        return true;
    }
};

template<>
struct convert<DatabaseConfig> {
    static Node encode(const DatabaseConfig& rhs) {
        Node node;
        node["host"] = rhs.host;
        node["port"] = rhs.port;
        node["name"] = rhs.name;
        node["user"] = rhs.user;
        node["password"] = rhs.password;
        node["pool_size"] = rhs.pool_size;
        return node;
    }

    static bool decode(const Node& node, DatabaseConfig& rhs) {
        if (!node.IsMap()) return false;
        rhs.host = node["host"].as<std::string>("localhost");
        rhs.port = node["port"].as<uint16_t>(5432);
        rhs.name = node["name"].as<std::string>("vaulthalla");
        rhs.user = node["user"].as<std::string>("vaulthalla");
        rhs.password = node["password"].as<std::string>("changeme");
        rhs.pool_size = node["pool_size"].as<int>(10);
        return true;
    }
};

// AuthConfig

template<>
struct convert<AuthConfig> {
    static Node encode(const AuthConfig& rhs) {
        Node node;
        node["token_expiry_minutes"] = rhs.token_expiry_minutes;
        node["refresh_token_expiry_days"] = rhs.refresh_token_expiry_days;
        return node;
    }

    static bool decode(const Node& node, AuthConfig& rhs) {
        if (!node.IsMap()) return false;
        rhs.token_expiry_minutes = node["token_expiry_minutes"].as<int>(60);
        rhs.refresh_token_expiry_days = node["refresh_token_expiry_days"].as<int>(7);
        return true;
    }
};

template<>
struct convert<SharingConfig> {
    static Node encode(const SharingConfig& rhs) {
        Node node;
        node["enabled"] = rhs.enabled;
        node["enable_public_links"] = rhs.enable_public_links;
        return node;
    }

    static bool decode(const Node& node, SharingConfig& rhs) {
        if (!node.IsMap()) return false;
        rhs.enabled = node["enabled"].as<bool>(true);
        rhs.enable_public_links = node["enable_public_links"].as<bool>(true);
        return true;
    }
};

template<>
struct convert<DevConfig> {
    static Node encode(const DevConfig& rhs) {
        Node node;
        node["enabled"] = rhs.enabled;
        node["init_r2_test_vault"] = rhs.init_r2_test_vault;
        return node;
    }

    static bool decode(const Node& node, DevConfig& rhs) {
        if (!node.IsMap()) return false;
        rhs.enabled = node["enabled"].as<bool>(false);
        rhs.init_r2_test_vault = node["init_r2_test_vault"].as<bool>(false);
        return true;
    }
};

template<>
struct convert<std::filesystem::path> {
    static Node encode(const std::filesystem::path& rhs) {
        return Node(rhs.string());
    }

    static bool decode(const Node& node, std::filesystem::path& rhs) {
        if (!node.IsScalar()) return false;
        rhs = std::filesystem::path(node.as<std::string>());
        return true;
    }
};

} // namespace YAML
