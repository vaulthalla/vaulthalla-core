#include "types/Vault.hpp"
#include "types/S3Vault.hpp"
#include "util/timestamp.hpp"
#include "util/cmdLineHelpers.hpp"

#include <nlohmann/json.hpp>
#include <pqxx/row>
#include <fmt/core.h>

using namespace vh::types;
using namespace vh::shell;

std::string vh::types::to_string(const VaultType& type) {
    switch (type) {
        case VaultType::Local: return "local";
        case VaultType::S3: return "s3";
        default: return "unknown";
    }
}

VaultType vh::types::from_string(const std::string& type) {
    if (type == "local") return VaultType::Local;
    if (type == "s3") return VaultType::S3;
    throw std::invalid_argument("Invalid VaultType: " + type);
}

Vault::Vault(const pqxx::row& row)
    : id(row["id"].as<unsigned int>()),
      name(row["name"].as<std::string>()),
      description(row["description"].as<std::string>()),
      owner_id(row["owner_id"].as<unsigned int>()),
      quota(row["quota"].as<unsigned long long>()),
      type(from_string(row["type"].as<std::string>())),
      mount_point(std::filesystem::path(row["mount_point"].as<std::string>())),
      is_active(row["is_active"].as<bool>()),
      created_at(util::parsePostgresTimestamp(row["created_at"].c_str())) {}

void vh::types::to_json(nlohmann::json& j, const Vault& v) {
    j = {
        {"id", v.id},
        {"name", v.name},
        {"type", to_string(v.type)},
        {"description", v.description},
        {"quota", v.quota},
        {"owner_id", v.owner_id},
        {"mount_point", v.mount_point.string()},
        {"is_active", v.is_active},
        {"created_at", util::timestampToString(v.created_at)}
    };
}

void vh::types::from_json(const nlohmann::json& j, Vault& v) {
    v.id = j.at("id").get<unsigned int>();
    v.name = j.at("name").get<std::string>();
    v.description = j.at("description").get<std::string>();
    v.quota = j.at("quota").get<unsigned long long>();
    v.type = from_string(j.at("type").get<std::string>());
    v.owner_id = j.at("owner_id").get<unsigned int>();
    v.mount_point = std::filesystem::path(j.at("mount_point").get<std::string>());
    v.is_active = j.at("is_active").get<bool>();
    v.created_at = util::parseTimestampFromString(j.at("created_at").get<std::string>());
}

void vh::types::to_json(nlohmann::json& j, const std::vector<std::shared_ptr<Vault>>& vaults) {
    j = nlohmann::json::array();
    for (const auto& vault : vaults) {
        if (const auto* s3 = dynamic_cast<const S3Vault*>(vault.get())) j.push_back(*s3);
        else j.push_back(*vault);
    }
}

std::string vh::types::to_string(const Vault& v) {
    if (v.quota == 0) return fmt::format("{} ({}): {} - {} at {}",
                           v.name,
                           to_string(v.type),
                           v.description,
                           "\u221E",  // ∞ symbol
                           v.mount_point.string());

    return fmt::format("{} ({}): {} - {} ({} bytes) at {}",
                       v.name,
                       to_string(v.type),
                       v.description,
                       human_bytes(v.quota),
                       static_cast<unsigned long long>(v.quota),
                       v.mount_point.string());
}

std::string vh::types::to_string(const std::shared_ptr<Vault>& v) {
    if (!v) return "null";
    return to_string(*v);
}

std::string vh::types::to_string(const std::vector<std::shared_ptr<Vault>>& vaults) {
    if (vaults.empty()) return "No vaults found\n";

    struct Row { std::string name, type, quota, mount, desc; };
    std::vector<Row> rows;
    rows.reserve(vaults.size());

    // Build rows (materialize strings once)
    size_t name_w = 4;      // "NAME"
    size_t type_w = 4;      // "TYPE"
    size_t quota_w = 5;     // "QUOTA"
    size_t mount_w = 5;     // "MOUNT"

    for (const auto& vp : vaults) {
        const Vault& v = *vp;
        Row r;
        r.name  = v.name;
        r.type  = to_string(v.type);
        r.desc  = v.description;

        if (v.quota == 0) r.quota = "∞";  // Use infinity symbol for no quota
        else r.quota = fmt::format("{} ({})", human_bytes(static_cast<uint64_t>(v.quota)),
                              static_cast<unsigned long long>(v.quota));
        r.mount = v.mount_point.string();

        name_w  = std::max(name_w,  r.name.size());
        type_w  = std::max(type_w,  r.type.size());
        quota_w = std::max(quota_w, r.quota.size());
        mount_w = std::max(mount_w, r.mount.size());

        rows.push_back(std::move(r));
    }

    // Keep mount column reasonable so DESCRIPTION has breathing room
    const int term = term_width();
    const size_t hard_mount_cap = 48;                       // adjust to taste
    mount_w = std::min(mount_w, hard_mount_cap);

    // Compute description width given terminal width
    const size_t left = 2 + name_w + 2 + type_w + 2 + quota_w + 2 + mount_w + 2;
    const size_t desc_width = (term > static_cast<int>(left + 10))
                                ? (static_cast<size_t>(term) - left)
                                : 40;                      // fallback if no TTY or tiny width

    // Header
    std::string out;
    out.reserve(96 + rows.size() * 96);
    out += "vaulthalla vaults:\n";
    out += fmt::format("  {:{}}  {:{}}  {:{}}  {:{}}  {}\n",
                       "NAME", name_w, "TYPE", type_w, "QUOTA", quota_w, "MOUNT", mount_w, "DESCRIPTION");
    out += fmt::format("  {:-<{}}  {:-<{}}  {:-<{}}  {:-<{}}  {}\n",
                       "", name_w, "", type_w, "", quota_w, "", mount_w, "-----------");

    // Rows
    for (auto& r : rows) {
        // Clamp mount to column width with ellipsis if needed
        std::string mount_col = (r.mount.size() > mount_w) ? ellipsize_middle(r.mount, mount_w) : r.mount;

        // Wrap description to desc_width
        auto wrapped = wrap_text(r.desc, desc_width);
        // First line: print everything
        out += fmt::format("  {:{}}  {:{}}  {:{}}  {:{}}  {}\n",
                           r.name, name_w, r.type, type_w, r.quota, quota_w, mount_col, mount_w,
                           wrapped.substr(0, wrapped.find('\n')));

        // Continuations: description only
        size_t pos = wrapped.find('\n');
        while (pos != std::string::npos) {
            const size_t next = wrapped.find('\n', pos + 1);
            auto line = wrapped.substr(pos + 1, next - (pos + 1));
            out += fmt::format("  {:{}}  {:{}}  {:{}}  {:{}}  {}\n",
                               "", name_w, "", type_w, "", quota_w, "", mount_w, line);
            pos = next;
        }
        out += '\n';  // Add a blank line between vaults
    }

    return out;
}
