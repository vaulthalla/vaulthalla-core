#include "types/db/Role.hpp"
#include "util/timestamp.hpp"
#include "types/db/Permission.hpp"

#include <nlohmann/json.hpp>

using namespace vh::types;

Role::Role(const pqxx::row& row)
    : id(row["id"].as<unsigned int>()),
      name(row["name"].as<std::string>()),
      display_name(row["display_name"].as<std::string>()),
      description(row["description"].as<std::string>()),
      permissions(row["permissions"].as<uint16_t>()),
      created_at(util::parsePostgresTimestamp(row["created_at"].as<std::string>())) {}

Role::Role(const nlohmann::json& j)
    : id(j.at("id").get<unsigned int>()),
      name(j.at("name").get<std::string>()),
      description(j.at("description").get<std::string>()),
      permissions(toBitmask(j.at("permissions").get<std::vector<PermissionName> >())),
      created_at(util::parsePostgresTimestamp(j.at("created_at").get<std::string>())) {}

void vh::types::to_json(nlohmann::json& j, const Role& r) {
    j = {
        {"id", r.id},
        {"name", r.name},
        {"display_name", r.display_name},
        {"description", r.description},
        {"permissions", permsFromBitmaskAsString(r.permissions)},
        {"created_at", util::timestampToString(r.created_at)},
    };
}

void vh::types::from_json(const nlohmann::json& j, Role& r) {
    r.id = j.at("id").get<unsigned int>();
    r.name = j.at("name").get<std::string>();
    r.description = j.at("description").get<std::string>();
    r.permissions = toBitmask(j.at("permissions").get<std::vector<PermissionName> >());
    r.created_at = util::parsePostgresTimestamp(j.at("created_at").get<std::string>());
}

std::vector<std::shared_ptr<Role>> vh::types::roles_from_json(const nlohmann::json& j) {
    std::vector<std::shared_ptr<Role>> roles;
    for (const auto& roleJson : j) roles.push_back(std::make_shared<Role>(roleJson));
    return roles;
}

void vh::types::to_json(nlohmann::json& j, const std::vector<std::shared_ptr<Role>>& roles) {
    j = nlohmann::json::array();
    for (const auto& role : roles) j.push_back(*role);
}

nlohmann::json from_json(const std::vector<std::shared_ptr<Role>>& roles) {
    nlohmann::json j = nlohmann::json::array();
    for (const auto& role : roles) j.push_back(*role);
    return j;
}

bool Role::canManageUsers() const { return hasPermission(permissions, PermissionName::ManageUsers); }
bool Role::canManageRoles() const { return hasPermission(permissions, PermissionName::ManageRoles); }
bool Role::canManageStorage() const { return hasPermission(permissions, PermissionName::ManageStorage); }
bool Role::canManageFiles() const { return hasPermission(permissions, PermissionName::ManageFiles); }
bool Role::canViewAuditLog() const { return hasPermission(permissions, PermissionName::ViewAuditLog); }
bool Role::canUploadFile() const { return hasPermission(permissions, PermissionName::UploadFile); }
bool Role::canDownloadFile() const { return hasPermission(permissions, PermissionName::DownloadFile); }
bool Role::canDeleteFile() const { return hasPermission(permissions, PermissionName::DeleteFile); }
bool Role::canShareFile() const { return hasPermission(permissions, PermissionName::ShareFile); }
bool Role::canLockFile() const { return hasPermission(permissions, PermissionName::LockFile); }
bool Role::canManageSettings() const { return hasPermission(permissions, PermissionName::ManageSettings); }
