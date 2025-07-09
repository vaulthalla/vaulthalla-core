#include "config/Config.hpp"
#include "config/config_yaml.hpp"

#include <cstdlib>
#include <iostream>
#include <yaml-cpp/yaml.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>

namespace vh::config {

template <typename T> T getOrDefault(const YAML::Node& node, const std::string& key, const T& def) {
    return node[key] ? node[key].as<T>() : def;
}

Config loadConfig(const std::string& path) {
    Config cfg;
    YAML::Node root = YAML::LoadFile(path);

    if (auto node = root["server"])    YAML::convert<ServerConfig>::decode(node, cfg.server);
    if (auto node = root["fuse"])      YAML::convert<FuseConfig>::decode(node, cfg.fuse);
    if (auto node = root["cloud"])     YAML::convert<CloudConfig>::decode(node, cfg.cloud);
    if (auto node = root["database"])  YAML::convert<DatabaseConfig>::decode(node, cfg.database);
    if (auto node = root["auth"])      YAML::convert<AuthConfig>::decode(node, cfg.auth);
    if (auto node = root["metrics"])   YAML::convert<MetricsConfig>::decode(node, cfg.metrics);
    if (auto node = root["admin_ui"])  YAML::convert<AdminUIConfig>::decode(node, cfg.admin_ui);
    if (auto node = root["scheduler"]) YAML::convert<SchedulerConfig>::decode(node, cfg.scheduler);
    if (auto node = root["advanced"])  YAML::convert<AdvancedConfig>::decode(node, cfg.advanced);

    // 👇 Handle ENV overrides separately
    if (const char* pw = std::getenv("VAULTHALLA_DB_PASSWORD")) cfg.database.password = pw;
    if (const char* jwt = std::getenv("VAULTHALLA_JWT_SECRET")) cfg.auth.jwt_secret = jwt;

    return cfg;
}

void Config::save() const {
    using namespace std;
    namespace fs = std::filesystem;

    const fs::path configFile = CONFIG_FILE_PATH / "config.yaml";
    const fs::path templateFile = CONFIG_FILE_PATH / "config_template.yaml.in";

    // Read in the static template with comments
    ifstream templateIn(templateFile);
    if (!templateIn.is_open()) throw runtime_error("Failed to open config template file");

    stringstream buffer;
    buffer << templateIn.rdbuf();
    string templateContent = buffer.str();

    // Convert each section to YAML
    const auto encode = []<typename T>(const T& section) -> std::string {
        YAML::Emitter out;
        out << YAML::convert<T>::encode(section);
        return std::string(out.c_str());
    };

    unordered_map<string, string> sectionMap = {
        { "server",     encode(server) },
        { "fuse",       encode(fuse) },
        { "cloud",      encode(cloud) },
        { "database",   encode(database) },
        { "auth",       encode(auth) },
        { "metrics",    encode(metrics) },
        { "admin_ui",   encode(admin_ui) },
        { "scheduler",  encode(scheduler) },
        { "advanced",   encode(advanced) }
    };

    // Replace section stubs
    for (const auto& [key, yaml] : sectionMap) {
        string searchKey = key + ": {}";
        string replacement = key + ":\n" + string(yaml);
        if (const auto& pos = templateContent.find(searchKey) != string::npos)
            templateContent.replace(pos, searchKey.length(), replacement);
    }

    // Write the final result
    ofstream out(configFile);
    if (!out.is_open()) {
        throw runtime_error("Failed to write config file");
    }

    out << templateContent;
    out.close();
}

void to_json(nlohmann::json& j, const Config& c) {
    j = nlohmann::json{
        {"server", {
            {"host", c.server.host},
            {"port", c.server.port},
            {"uds_socket", c.server.uds_socket},
            {"log_level", c.server.log_level},
            {"max_connections", c.server.max_connections}
        }},
        {"fuse", {
            {"enabled", c.fuse.enabled},
            {"root_mount_path", c.fuse.root_mount_path},
            {"mount_per_user", c.fuse.mount_per_user},
            {"fuse_timeout_seconds", c.fuse.fuse_timeout_seconds},
            {"allow_other", c.fuse.allow_other}
        }},
        {"cloud", {
            {"enabled", c.cloud.enabled},
            {"cache", {
                {"enabled", c.cloud.cache.enabled},
                {"expiry_days", c.cloud.cache.expiry_days},
                {"thumbnails_only", c.cloud.cache.thumbnails_only},
                {"cache_path", c.cloud.cache.cache_path}
            }}
        }},
        {"database", {
            {"host", c.database.host},
            {"port", c.database.port},
            {"name", c.database.name},
            {"user", c.database.user},
            {"password", c.database.password}, // Sensitive, handle with care
            {"pool_size", c.database.pool_size}
        }},
        {"auth", {
            {"token_expiry_minutes", c.auth.token_expiry_minutes},
            {"refresh_token_expiry_days", c.auth.refresh_token_expiry_days},
            {"jwt_secret", c.auth.jwt_secret}, // Sensitive, handle with care
            {"allow_signup", c.auth.allow_signup}
        }},
        {"metrics", {
            {"enabled", c.metrics.enabled},
            {"port", c.metrics.port}
        }},
        {"admin_ui", {
            {"enabled", c.admin_ui.enabled},
            {"bind_port", c.admin_ui.bind_port},
            {"allowed_ips", c.admin_ui.allowed_ips}
        }},
        {"scheduler", {
            {"cleanup_interval_hours", c.scheduler.cleanup_interval_hours},
            {"audit_prune_days", c.scheduler.audit_prune_days},
            {"usage_refresh_minutes", c.scheduler.usage_refresh_minutes}
        }},
        {"advanced", {
            {"enable_file_versioning", c.advanced.enable_file_versioning},
            {"max_upload_size_mb", c.advanced.max_upload_size_mb},
            {"enable_sharing", c.advanced.enable_sharing},
            {"enable_public_links", c.advanced.enable_public_links},
            {"rate_limit_per_ip_per_minute", c.advanced.rate_limit_per_ip_per_minute},
            {"dev_mode", c.advanced.dev_mode}
        }}
    };
}

void from_json(const nlohmann::json& j, Config& c) {
    c.server.host = j.at("server").at("host").get<std::string>();
    c.server.port = j.at("server").at("port").get<uint16_t>();
    c.server.uds_socket = j.at("server").at("uds_socket").get<std::string>();
    c.server.log_level = j.at("server").at("log_level").get<std::string>();
    c.server.max_connections = j.at("server").at("max_connections").get<uint16_t>();

    c.fuse.enabled = j.at("fuse").at("enabled").get<bool>();
    c.fuse.root_mount_path = j.at("fuse").at("root_mount_path").get<std::string>();
    c.fuse.mount_per_user = j.at("fuse").at("mount_per_user").get<bool>();
    c.fuse.fuse_timeout_seconds = j.at("fuse").at("fuse_timeout_seconds").get<int>();
    c.fuse.allow_other = j.at("fuse").at("allow_other").get<bool>();

    c.cloud.enabled = j.at("cloud").at("enabled").get<bool>();
    c.cloud.cache = CloudCacheConfig();
    c.cloud.cache.enabled = j.at("cloud").at("cache").at("enabled").get<bool>();
    c.cloud.cache.expiry_days = j.at("cloud").at("cache").at("expiry_days").get<int>();
    c.cloud.cache.thumbnails_only = j.at("cloud").at("cache").value("thumbnails_only", false);
    c.cloud.cache.cache_path = j.at("cloud").at("cache").at("cache_path").get<std::string>();

    c.database.host = j.at("database").at("host").get<std::string>();
    c.database.port = j.at("database").at("port").get<uint16_t>();
    c.database.name = j.at("database").at("name").get<std::string>();
    c.database.user = j.at("database").at("user").get<std::string>();
    c.database.password = j.at("database").at("password").get<std::string>(); // Sensitive, handle with care
    c.database.pool_size = j.at("database").at("pool_size").get<uint16_t>();

    c.auth.token_expiry_minutes = j.at("auth").at("token_expiry_minutes").get<int>();
    c.auth.refresh_token_expiry_days = j.at("auth").at("refresh_token_expiry_days").get<int>();
    c.auth.jwt_secret = j.at("auth").at("jwt_secret").get<std::string>(); // Sensitive, handle with care
    c.auth.allow_signup = j.at("auth").at("allow_signup").get<bool>();

    c.metrics.enabled = j.at("metrics").at("enabled").get<bool>();
    c.metrics.port = j.at("metrics").at("port").get<uint16_t>();

    c.admin_ui.enabled = j.at("admin_ui").at("enabled").get<bool>();
    c.admin_ui.bind_port = j.at("admin_ui").at("bind_port").get<uint16_t>();
    c.admin_ui.allowed_ips = j.at("admin_ui").at("allowed_ips").get<std::vector<std::string>>();

    c.scheduler.cleanup_interval_hours = j.at("scheduler").at("cleanup_interval_hours").get<int>();
    c.scheduler.audit_prune_days = j.at("scheduler").at("audit_prune_days").get<int>();
    c.scheduler.usage_refresh_minutes = j.at("scheduler").at("usage_refresh_minutes").get<int>();

    c.advanced.enable_file_versioning = j.at("advanced").at("enable_file_versioning").get<bool>();
    c.advanced.max_upload_size_mb = j.at("advanced").at("max_upload_size_mb").get<int>();
    c.advanced.enable_sharing = j.at("advanced").at("enable_sharing").get<bool>();
    c.advanced.enable_public_links = j.at("advanced").at("enable_public_links").get<bool>();
    c.advanced.rate_limit_per_ip_per_minute = j.at("advanced").at("rate_limit_per_ip_per_minute").get<int>();
    c.advanced.dev_mode = j.at("advanced").at("dev_mode").get<bool>();
}

} // namespace vh::config
