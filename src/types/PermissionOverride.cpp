#include "types/PermissionOverride.hpp"
#include "protocols/shell/Table.hpp"
#include "util/cmdLineHelpers.hpp"

#include <pqxx/result>
#include <nlohmann/json.hpp>
#include <cctype>

using namespace vh::types;
using namespace vh::shell;

PermissionOverride::PermissionOverride(const pqxx::row& row)
    : permission(row),
      is_file(row["is_file"].as<bool>()),
      enabled(row["enabled"].as<bool>()),
      patternStr(row["regex"].as<std::string>()),
      pattern(patternStr) {}

PermissionOverride::PermissionOverride(const nlohmann::json& j)
    : permission(j.at("permission")),
      is_file(j.value("is_file", false)),
      enabled(j.value("enabled", false)),
      patternStr(j.value("regex", std::string())),
      pattern(patternStr) {
    if (!j.contains("permission")) throw std::runtime_error("PermissionOverride JSON must contain 'permission'");
}

void vh::types::to_json(nlohmann::json& j, const PermissionOverride& po) {
    j = {
        {"is_file", po.is_file},
        {"enabled", po.enabled},
        {"regex", po.patternStr},
        {"permission", po.permission}
    };
}

void vh::types::from_json(const nlohmann::json& j, PermissionOverride& po) {
    po.is_file = j.at("is_file").get<bool>();
    po.enabled = j.at("enabled").get<bool>();
    po.patternStr = j.at("regex").get<std::string>();
    po.pattern = std::regex(po.patternStr);
    from_json(j.at("permission"), po.permission);
}

std::vector<std::shared_ptr<PermissionOverride>> vh::types::permissionOverridesFromPqRes(const pqxx::result& res) {
    std::vector<std::shared_ptr<PermissionOverride>> overrides;
    for (const auto& row : res) overrides.push_back(std::make_shared<PermissionOverride>(row));
    return overrides;
}

std::vector<std::shared_ptr<PermissionOverride>> vh::types::permissionOverridesFromJson(const nlohmann::json& j) {
    std::vector<std::shared_ptr<PermissionOverride>> overrides;
    for (const auto& item : j) overrides.push_back(std::make_shared<PermissionOverride>(item));
    return overrides;
}

void vh::types::to_json(nlohmann::json& j, const std::vector<std::shared_ptr<PermissionOverride>>& overrides) {
    j = nlohmann::json::array();
    for (const auto& overridePtr : overrides) {
        nlohmann::json overrideJson;
        to_json(overrideJson, *overridePtr);
        j.push_back(overrideJson);
    }
}

std::string vh::types::to_string(const std::vector<std::shared_ptr<PermissionOverride>>& overrides) {
    if (overrides.empty()) return "No overrides";

    Table tbl({
        {"Name", Align::Left, 4, 32, false, false},
        {"Description", Align::Left, 4, 64, false, false},
        {"Value", Align::Left, 4, 8, false, false},
        {"Regex", Align::Left, 4, 64, false, false}
    }, term_width());

    for (const auto& ovr : overrides) {
        tbl.add_row({
            ovr->permission.name,
            ovr->permission.description,
            ovr->enabled ? "On" : "Off",
            ovr->patternStr
        });
    }

    return tbl.render();
}

std::string vh::types::to_string(const OverrideOpt& opt) {
    switch (opt) {
        case OverrideOpt::ALLOW: return "allow";
        case OverrideOpt::DENY: return "deny";
        default: return "unknown";
    }
}

OverrideOpt vh::types::overrideOptFromString(const std::string& str) {
    auto lwr = str;
    std::ranges::transform(lwr.begin(), lwr.end(), lwr.begin(),
        [](const unsigned char c) { return std::tolower(c); });
    if (lwr == "allow") return OverrideOpt::ALLOW;
    if (lwr == "deny") return OverrideOpt::DENY;
    throw std::invalid_argument("Invalid OverrideOpt string: " + lwr);
}
