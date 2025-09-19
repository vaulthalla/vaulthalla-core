#pragma once

#include "Permission.hpp"

#include <regex>
#include <nlohmann/json_fwd.hpp>

namespace pqxx {
    class row;
    class result;
}

namespace vh::types {

enum class OverrideOpt { ALLOW, DENY };

std::string to_string(const OverrideOpt& opt);
OverrideOpt overrideOptFromString(const std::string& str);

struct PermissionOverride {
    unsigned int id{0};
    Permission permission;
    OverrideOpt effect{OverrideOpt::ALLOW};
    unsigned int assignment_id{0}; // ID of the vault_role_assignment this override is assigned to
    bool enabled{true};
    std::string patternStr;
    std::regex pattern;

    PermissionOverride() = default;
    explicit PermissionOverride(const pqxx::row& row);
    explicit PermissionOverride(const nlohmann::json& j);
};

void to_json(nlohmann::json& j, const PermissionOverride& po);
void from_json(const nlohmann::json& j, PermissionOverride& po);

std::vector<std::shared_ptr<PermissionOverride>> permissionOverridesFromPqRes(const pqxx::result& res);
std::vector<std::shared_ptr<PermissionOverride>> permissionOverridesFromJson(const nlohmann::json& j);
void to_json(nlohmann::json& j, const std::vector<std::shared_ptr<PermissionOverride>>& overrides);

std::string to_string(const std::shared_ptr<PermissionOverride>& override);
std::string to_string(const std::vector<std::shared_ptr<PermissionOverride>>& overrides);

}
