#include "types/Role.hpp"
#include "types/PermissionOverride.hpp"
#include "util/timestamp.hpp"

#include <pqxx/row>
#include <pqxx/result>
#include <nlohmann/json.hpp>

using namespace vh::types;

Role::Role(const pqxx::row& row)
    : name(row["name"].as<std::string>()),
      description(row["description"].as<std::string>()),
      type(row["type"].as<std::string>()),
      created_at(util::parsePostgresTimestamp(row["created_at"].as<std::string>())),
      permissions(static_cast<uint16_t>(row["permissions"].as<int64_t>())) {
    if (!row["role_id"].is_null()) id = row["role_id"].as<unsigned int>();
    else if (!row["id"].is_null()) id = row["id"].as<unsigned int>();
    else throw std::runtime_error("Role row does not contain 'role_id' or 'id'");
}

Role::Role(const nlohmann::json& j)
    : id(j.contains("role_id") ? j.at("role_id").get<unsigned int>() : 0),
      name(j.at("name").get<std::string>()),
      description(j.at("description").get<std::string>()),
      type(j.at("type").get<std::string>()),
      created_at(static_cast<std::time_t>(0)),
      permissions(type == "user" ? adminMaskFromJson(j.at("permissions")) : vaultMaskFromJson(j.at("permissions"))) {}

Role::Role(std::string name, std::string description, std::string type, const uint16_t permissions)
    : name(std::move(name)),
      description(std::move(description)),
      type(std::move(type)),
      permissions(permissions) {}

void vh::types::to_json(nlohmann::json& j, const Role& r) {
    j = {
        {"role_id", r.id},
        {"name", r.name},
        {"description", r.description},
        {"type", r.type},
        {"permissions", r.type == "user" ? jsonFromAdminMask(r.permissions) : jsonFromVaultMask(r.permissions)},
        {"created_at", util::timestampToString(r.created_at)}
    };
}

void vh::types::from_json(const nlohmann::json& j, Role& r) {
    if (j.contains("role_id")) r.id = j.at("role_id").get<unsigned int>();
    r.name = j.at("name").get<std::string>();
    r.description = j.at("description").get<std::string>();
    r.type = j.at("type").get<std::string>();
    r.permissions = r.type == "user" ? adminMaskFromJson(j.at("permissions")) : vaultMaskFromJson(j.at("permissions"));
    r.created_at = util::parsePostgresTimestamp(j.at("created_at").get<std::string>());
}

void vh::types::to_json(nlohmann::json& j, const std::vector<std::shared_ptr<Role>>& roles) {
    j = nlohmann::json::array();
    for (const auto& role : roles) j.push_back(*role);
}

std::vector<std::shared_ptr<Role> > vh::types::roles_from_pq_res(const pqxx::result& res) {
    std::vector<std::shared_ptr<Role> > roles;
    for (const auto& item : res) roles.push_back(std::make_shared<Role>(item));
    return roles;
}
