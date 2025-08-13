#pragma once

#include "Permission.hpp"

#include <regex>
#include <nlohmann/json_fwd.hpp>

namespace pqxx {
    class row;
    class result;
}

namespace vh::types {

struct PermissionOverride {
    Permission permission;
    bool is_file{false}, enabled{false};
    std::string patternStr;
    std::regex pattern;

    explicit PermissionOverride(const pqxx::row& row);
    explicit PermissionOverride(const nlohmann::json& j);
};

void to_json(nlohmann::json& j, const PermissionOverride& po);
void from_json(const nlohmann::json& j, PermissionOverride& po);

std::vector<std::shared_ptr<PermissionOverride>> permissionOverridesFromPqRes(const pqxx::result& res);
std::vector<std::shared_ptr<PermissionOverride>> permissionOverridesFromJson(const nlohmann::json& j);
void to_json(nlohmann::json& j, const std::vector<std::shared_ptr<PermissionOverride>>& overrides);

std::string to_string(const std::vector<std::shared_ptr<PermissionOverride>>& overrides);

}
