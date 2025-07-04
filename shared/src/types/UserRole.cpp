#include "types/UserRole.hpp"
#include "util/timestamp.hpp"
#include <pqxx/row>
#include <nlohmann/json.hpp>

using namespace vh::types;

UserRole::UserRole(const pqxx::row& row)
    : id(row["role_id"].as<unsigned int>()),
      name(row["role_name"].as<std::string>()),
      description(row["role_description"].as<std::string>()),
      created_at(util::parsePostgresTimestamp(row["role_created_at"].as<std::string>())),
      permissions(static_cast<uint16_t>(row["role_permissions"].as<int64_t>())) {}

UserRole::UserRole(const nlohmann::json& j)
    : id(j.contains("id") ? j.at("id").get<unsigned int>() : 0),
      name(j.at("name").get<std::string>()),
      description(j.at("description").get<std::string>()),
      created_at(static_cast<std::time_t>(0)),
      permissions(adminMaskFromJson(j.at("permissions"))) {}

void vh::types::to_json(nlohmann::json& j, const UserRole& r) {
    j = {
        {"id", r.id},
        {"name", r.name},
        {"description", r.description},
        {"permissions", jsonFromAdminMask(r.permissions)},
        {"created_at", util::timestampToString(r.created_at)}
    };
}

void vh::types::from_json(const nlohmann::json& j, UserRole& r) {
    if (j.contains("id")) r.id = j.at("id").get<unsigned int>();
    r.name = j.at("name").get<std::string>();
    r.description = j.at("description").get<std::string>();
    r.permissions = adminMaskFromJson(j.at("permissions"));
}
