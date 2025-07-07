#pragma once

#include "config/Config.hpp"
#include <yaml-cpp/yaml.h>

namespace YAML {

using namespace vh::config;

// ServerConfig

template<>
struct convert<ServerConfig> {
    static Node encode(const ServerConfig& rhs) {
        Node node;
        node["host"] = rhs.host;
        node["port"] = rhs.port;
        node["uds_socket"] = rhs.uds_socket;
        node["log_level"] = rhs.log_level;
        node["max_connections"] = rhs.max_connections;
        return node;
    }

    static bool decode(const Node& node, ServerConfig& rhs) {
        if (!node.IsMap()) return false;
        rhs.host = node["host"].as<std::string>("0.0.0.0");
        rhs.port = node["port"].as<uint16_t>(8080);
        rhs.uds_socket = node["uds_socket"].as<std::string>("/tmp/vaulthalla.sock");
        rhs.log_level = node["log_level"].as<std::string>("info");
        rhs.max_connections = node["max_connections"].as<int>(1024);
        return true;
    }
};

// FuseConfig

template<>
struct convert<FuseConfig> {
    static Node encode(const FuseConfig& rhs) {
        Node node;
        node["enabled"] = rhs.enabled;
        node["root_mount_path"] = rhs.root_mount_path;
        node["mount_per_user"] = rhs.mount_per_user;
        node["fuse_timeout_seconds"] = rhs.fuse_timeout_seconds;
        node["allow_other"] = rhs.allow_other;
        return node;
    }

    static bool decode(const Node& node, FuseConfig& rhs) {
        if (!node.IsMap()) return false;
        rhs.enabled = node["enabled"].as<bool>(true);
        rhs.root_mount_path = node["root_mount_path"].as<std::string>("/mnt/vaulthalla");
        rhs.mount_per_user = node["mount_per_user"].as<bool>(true);
        rhs.fuse_timeout_seconds = node["fuse_timeout_seconds"].as<int>(60);
        rhs.allow_other = node["allow_other"].as<bool>(true);
        return true;
    }
};

// DatabaseConfig

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
        node["jwt_secret"] = rhs.jwt_secret;
        node["allow_signup"] = rhs.allow_signup;
        return node;
    }

    static bool decode(const Node& node, AuthConfig& rhs) {
        if (!node.IsMap()) return false;
        rhs.token_expiry_minutes = node["token_expiry_minutes"].as<int>(60);
        rhs.refresh_token_expiry_days = node["refresh_token_expiry_days"].as<int>(7);
        rhs.jwt_secret = node["jwt_secret"].as<std::string>("changeme-very-secret");
        rhs.allow_signup = node["allow_signup"].as<bool>(false);
        return true;
    }
};

// MetricsConfig

template<>
struct convert<MetricsConfig> {
    static Node encode(const MetricsConfig& rhs) {
        Node node;
        node["enabled"] = rhs.enabled;
        node["port"] = rhs.port;
        return node;
    }

    static bool decode(const Node& node, MetricsConfig& rhs) {
        if (!node.IsMap()) return false;
        rhs.enabled = node["enabled"].as<bool>(true);
        rhs.port = node["port"].as<uint16_t>(9100);
        return true;
    }
};

// AdminUIConfig

template<>
struct convert<AdminUIConfig> {
    static Node encode(const AdminUIConfig& rhs) {
        Node node;
        node["enabled"] = rhs.enabled;
        node["bind_port"] = rhs.bind_port;
        node["allowed_ips"] = rhs.allowed_ips;
        return node;
    }

    static bool decode(const Node& node, AdminUIConfig& rhs) {
        if (!node.IsMap()) return false;
        rhs.enabled = node["enabled"].as<bool>(true);
        rhs.bind_port = node["bind_port"].as<uint16_t>(9090);
        if (node["allowed_ips"]) rhs.allowed_ips = node["allowed_ips"].as<std::vector<std::string>>();
        return true;
    }
};

// SchedulerConfig

template<>
struct convert<SchedulerConfig> {
    static Node encode(const SchedulerConfig& rhs) {
        Node node;
        node["cleanup_interval_hours"] = rhs.cleanup_interval_hours;
        node["audit_prune_days"] = rhs.audit_prune_days;
        node["usage_refresh_minutes"] = rhs.usage_refresh_minutes;
        return node;
    }

    static bool decode(const Node& node, SchedulerConfig& rhs) {
        if (!node.IsMap()) return false;
        rhs.cleanup_interval_hours = node["cleanup_interval_hours"].as<int>(24);
        rhs.audit_prune_days = node["audit_prune_days"].as<int>(90);
        rhs.usage_refresh_minutes = node["usage_refresh_minutes"].as<int>(10);
        return true;
    }
};

// AdvancedConfig

template<>
struct convert<AdvancedConfig> {
    static Node encode(const AdvancedConfig& rhs) {
        Node node;
        node["enable_file_versioning"] = rhs.enable_file_versioning;
        node["max_upload_size_mb"] = rhs.max_upload_size_mb;
        node["enable_sharing"] = rhs.enable_sharing;
        node["enable_public_links"] = rhs.enable_public_links;
        node["rate_limit_per_ip_per_minute"] = rhs.rate_limit_per_ip_per_minute;
        node["dev_mode"] = rhs.dev_mode;
        return node;
    }

    static bool decode(const Node& node, AdvancedConfig& rhs) {
        if (!node.IsMap()) return false;
        rhs.enable_file_versioning = node["enable_file_versioning"].as<bool>(true);
        rhs.max_upload_size_mb = node["max_upload_size_mb"].as<int>(2048);
        rhs.enable_sharing = node["enable_sharing"].as<bool>(true);
        rhs.enable_public_links = node["enable_public_links"].as<bool>(true);
        rhs.rate_limit_per_ip_per_minute = node["rate_limit_per_ip_per_minute"].as<int>(60);
        rhs.dev_mode = node["dev_mode"].as<bool>(false);
        return true;
    }
};

} // namespace YAML
