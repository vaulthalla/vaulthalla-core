#include "types/Config.hpp"
#include <yaml-cpp/yaml.h>
#include <cstdlib>
#include <iostream>

namespace vh::types::config {

template<typename T>
T getOrDefault(const YAML::Node& node, const std::string& key, const T& def) {
    return node[key] ? node[key].as<T>() : def;
}

Config loadConfig(const std::string& path) {
    Config cfg;
    YAML::Node root = YAML::LoadFile(path);

    if (auto node = root["server"]) {
        cfg.server.host = getOrDefault(node, "host", cfg.server.host);
        cfg.server.port = getOrDefault(node, "port", cfg.server.port);
        cfg.server.uds_socket = getOrDefault(node, "uds_socket", cfg.server.uds_socket);
        cfg.server.log_level = getOrDefault(node, "log_level", cfg.server.log_level);
        cfg.server.max_connections = getOrDefault(node, "max_connections", cfg.server.max_connections);
    }

    if (auto node = root["fuse"]) {
        cfg.fuse.enabled = getOrDefault(node, "enabled", cfg.fuse.enabled);
        cfg.fuse.root_mount_path = getOrDefault(node, "root_mount_path", cfg.fuse.root_mount_path);
        cfg.fuse.mount_per_user = getOrDefault(node, "mount_per_user", cfg.fuse.mount_per_user);
        cfg.fuse.fuse_timeout_seconds = getOrDefault(node, "fuse_timeout_seconds", cfg.fuse.fuse_timeout_seconds);
        cfg.fuse.allow_other = getOrDefault(node, "allow_other", cfg.fuse.allow_other);
    }

    if (auto node = root["database"]) {
        cfg.database.host = getOrDefault(node, "host", cfg.database.host);
        cfg.database.port = getOrDefault(node, "port", cfg.database.port);
        cfg.database.name = getOrDefault(node, "name", cfg.database.name);
        cfg.database.user = getOrDefault(node, "user", cfg.database.user);
        cfg.database.password = std::getenv("VAULTHALLA_DB_PASSWORD")
            ? std::getenv("VAULTHALLA_DB_PASSWORD")
            : getOrDefault(node, "password", cfg.database.password);
        cfg.database.pool_size = getOrDefault(node, "pool_size", cfg.database.pool_size);
    }

    if (auto node = root["storage"]) {
        cfg.storage.default_backend = getOrDefault(node, "default_backend", cfg.storage.default_backend);
        if (auto local = node["local"]) {
            cfg.storage.local.mount_point = getOrDefault(local, "mount_point", cfg.storage.local.mount_point);
        }
        if (auto s3 = node["s3"]) {
            cfg.storage.s3.endpoint = getOrDefault(s3, "endpoint", cfg.storage.s3.endpoint);
            cfg.storage.s3.region = getOrDefault(s3, "region", cfg.storage.s3.region);
            cfg.storage.s3.access_key = getOrDefault(s3, "access_key", cfg.storage.s3.access_key);
            cfg.storage.s3.secret_key = getOrDefault(s3, "secret_key", cfg.storage.s3.secret_key);
            cfg.storage.s3.bucket = getOrDefault(s3, "bucket", cfg.storage.s3.bucket);
        }
    }

    if (auto node = root["auth"]) {
        cfg.auth.token_expiry_minutes = getOrDefault(node, "token_expiry_minutes", cfg.auth.token_expiry_minutes);
        cfg.auth.refresh_token_expiry_days = getOrDefault(node, "refresh_token_expiry_days", cfg.auth.refresh_token_expiry_days);
        cfg.auth.jwt_secret = std::getenv("VAULTHALLA_JWT_SECRET")
            ? std::getenv("VAULTHALLA_JWT_SECRET")
            : getOrDefault(node, "jwt_secret", cfg.auth.jwt_secret);
        cfg.auth.allow_signup = getOrDefault(node, "allow_signup", cfg.auth.allow_signup);
    }

    if (auto node = root["tls"]) {
        cfg.tls.enabled = getOrDefault(node, "enabled", cfg.tls.enabled);
        cfg.tls.cert_file = getOrDefault(node, "cert_file", cfg.tls.cert_file);
        cfg.tls.key_file = getOrDefault(node, "key_file", cfg.tls.key_file);
    }

    if (auto node = root["metrics"]) {
        cfg.metrics.enabled = getOrDefault(node, "enabled", cfg.metrics.enabled);
        cfg.metrics.port = getOrDefault(node, "port", cfg.metrics.port);
    }

    if (auto node = root["admin_ui"]) {
        cfg.admin_ui.enabled = getOrDefault(node, "enabled", cfg.admin_ui.enabled);
        cfg.admin_ui.bind_port = getOrDefault(node, "bind_port", cfg.admin_ui.bind_port);
        if (node["allowed_ips"]) {
            for (const auto& ip : node["allowed_ips"]) {
                cfg.admin_ui.allowed_ips.push_back(ip.as<std::string>());
            }
        }
    }

    if (auto node = root["scheduler"]) {
        cfg.scheduler.cleanup_interval_hours = getOrDefault(node, "cleanup_interval_hours", cfg.scheduler.cleanup_interval_hours);
        cfg.scheduler.audit_prune_days = getOrDefault(node, "audit_prune_days", cfg.scheduler.audit_prune_days);
        cfg.scheduler.usage_refresh_minutes = getOrDefault(node, "usage_refresh_minutes", cfg.scheduler.usage_refresh_minutes);
    }

    if (auto node = root["advanced"]) {
        cfg.advanced.enable_file_versioning = getOrDefault(node, "enable_file_versioning", cfg.advanced.enable_file_versioning);
        cfg.advanced.max_upload_size_mb = getOrDefault(node, "max_upload_size_mb", cfg.advanced.max_upload_size_mb);
        cfg.advanced.enable_sharing = getOrDefault(node, "enable_sharing", cfg.advanced.enable_sharing);
        cfg.advanced.enable_public_links = getOrDefault(node, "enable_public_links", cfg.advanced.enable_public_links);
        cfg.advanced.rate_limit_per_ip_per_minute = getOrDefault(node, "rate_limit_per_ip_per_minute", cfg.advanced.rate_limit_per_ip_per_minute);
    }

    return cfg;
}

} // namespace vh::types::config
