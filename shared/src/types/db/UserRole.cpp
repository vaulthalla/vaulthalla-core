#include "types/db/UserRole.hpp"
#include "util/timestamp.hpp"
#include "types/db/Permission.hpp"

#include <nlohmann/json.hpp>
#include <pqxx/row>

using namespace vh::types;

UserRole::UserRole(const pqxx::row& row)
    : Role(row),
      id(row["id"].as<unsigned int>()),
      user_id(row["user_id"].as<unsigned int>()),
      role_id(row["role_id"].as<unsigned int>()),
      scope(row["scope"].as<std::string>()),
      scope_id(row["scope_id"].is_null() ? std::nullopt : std::make_optional(row["scope_id"].as<unsigned int>())),
      assigned_at(util::parsePostgresTimestamp(row["assigned_at"].as<std::string>())) {}

UserRole::UserRole(const nlohmann::json& row)
    : Role(row),
      id(row.at("id").get<unsigned int>()),
      user_id(row.at("user_id").get<unsigned int>()),
      role_id(row.at("role_id").get<unsigned int>()),
      scope(row.at("scope").get<std::string>()),
      scope_id(row.contains("scope_id") && !row["scope_id"].is_null()
                   ? std::make_optional(row.at("scope_id").get<unsigned int>())
                   : std::nullopt),
      assigned_at(util::parsePostgresTimestamp(row.at("assigned_at").get<std::string>())) {}

void vh::types::to_json(nlohmann::json& j, const UserRole& ur) {
    j = {
        {"id", ur.id},
        {"user_id", ur.user_id},
        {"role_id", ur.role_id},
        {"scope", ur.scope},
        {"assigned_at", util::timestampToString(ur.assigned_at)},
        {"name", ur.name},
        {"display_name", ur.display_name},
        {"description", ur.description},
        {"permissions", permsFromBitmaskAsString(ur.permissions)},
    };

    if (ur.scope_id.has_value()) j["scope_id"] = ur.scope_id.value();
}

void vh::types::from_json(const nlohmann::json& j, UserRole& ur) {
    ur.id = j.at("id").get<unsigned int>();
    ur.user_id = j.at("user_id").get<unsigned int>();
    ur.role_id = j.at("role_id").get<unsigned int>();
    ur.scope = j.at("scope").get<std::string>();
    ur.scope_id = j.contains("scope_id") && !j["scope_id"].is_null()
                      ? std::make_optional(j.at("scope_id").get<unsigned int>())
                      : std::nullopt;
    ur.assigned_at = util::parsePostgresTimestamp(j.at("assigned_at").get<std::string>());
    ur.name = j.at("name").get<std::string>();
    ur.display_name = j.at("display_name").get<std::string>();
    ur.description = j.at("description").get<std::string>();
    ur.permissions = toBitmask(j.at("permissions").get<std::vector<PermissionName>>());
}

void vh::types::to_json(nlohmann::json& j, const std::vector<std::shared_ptr<UserRole>>& user_roles) {
    j = nlohmann::json::array();
    for (const auto& ur : user_roles) j.push_back(*ur);
}

std::vector<std::shared_ptr<UserRole>> vh::types::user_roles_from_json(const nlohmann::json& j) {
    std::vector<std::shared_ptr<UserRole>> user_roles;
    for (const auto& urJson : j) user_roles.push_back(std::make_shared<UserRole>(urJson));
    return user_roles;
}
